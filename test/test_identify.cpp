#include "check.h"
#include <cstdint>
#include <string>

// Mocks
#define millis millis_identify
#define debugLogPrintf debugLogPrintf_identify
#define registryFindByIeee registryFindByIeee_identify
#define bulbReady bulbReady_identify
#define bulbSendOn bulbSendOn_identify
#define bulbSendOff bulbSendOff_identify

uint32_t mock_millis_identify = 0;
uint32_t millis() { return mock_millis_identify; }
void debugLogPrintf(const char * /*format*/, ...) {}

#include "bulb_registry.h"

Bulb mock_bulb;
Bulb mock_bulb2;

Bulb* registryFindByIeee(esp_zb_ieee_addr_t ieee) {
    if (memcmp(ieee, mock_bulb.ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
        return &mock_bulb;
    }
    if (memcmp(ieee, mock_bulb2.ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
        return &mock_bulb2;
    }
    return nullptr;
}

bool mock_bulb_ready = true;
bool mock_bulb2_ready = true;
bool bulbReady(Bulb *bulb) {
    if (bulb == &mock_bulb) return mock_bulb_ready;
    if (bulb == &mock_bulb2) return mock_bulb2_ready;
    return false;
}

int bulb_send_on_calls = 0;
void bulbSendOn(Bulb *bulb) {
    bulb_send_on_calls++;
    if (bulb) bulb->state.power = true;
}

int bulb_send_off_calls = 0;
void bulbSendOff(Bulb *bulb) {
    bulb_send_off_calls++;
    if (bulb) bulb->state.power = false;
}

#include "../identify.cpp"

void resetIdentifyTest() {
    mock_millis_identify = 0;
    mock_bulb_ready = true;
    mock_bulb2_ready = true;
    bulb_send_on_calls = 0;
    bulb_send_off_calls = 0;
    mock_bulb = Bulb();
    memset(mock_bulb.ieee, 0x11, sizeof(mock_bulb.ieee));
    mock_bulb.state.power = false; // Initially off

    mock_bulb2 = Bulb();
    memset(mock_bulb2.ieee, 0x22, sizeof(mock_bulb2.ieee));
    mock_bulb2.state.power = true; // Initially on

    identifyBegin();
}

void testIdentifyBulbValid() {
    check::begin("identifyBulb: full sequence");
    resetIdentifyTest();

    bool result = identifyBulb(&mock_bulb);
    CHECK(result);
    CHECK_EQ(bulb_send_on_calls, 1);
    CHECK_EQ(bulb_send_off_calls, 0);

    // Turn off
    mock_millis_identify += 500;
    identifyTick();
    CHECK_EQ(bulb_send_off_calls, 1);

    // Turn on
    mock_millis_identify += 400;
    identifyTick();
    CHECK_EQ(bulb_send_on_calls, 2);

    // Turn off
    mock_millis_identify += 500;
    identifyTick();
    CHECK_EQ(bulb_send_off_calls, 2);

    // Turn on
    mock_millis_identify += 400;
    identifyTick();
    CHECK_EQ(bulb_send_on_calls, 3);

    // Turn off
    mock_millis_identify += 500;
    identifyTick();
    CHECK_EQ(bulb_send_off_calls, 3);

    // 3 blinks complete, restores state (was off originally)
    // Wait, the last state before completion was "lit=false", so it called bulbSendOff
    // Then 400ms passes, --blinksLeft == 0, calls stopCurrent().
    // bulb->state.power is false, restorePower is false.
    // bulb->state.power != restorePower is FALSE.
    // So it does not call bulbSendOff again!
    mock_millis_identify += 400;
    identifyTick();
    CHECK_EQ(bulb_send_off_calls, 3);
    CHECK_EQ(bulb_send_on_calls, 3); // unchanged

    // Additional ticks do nothing
    mock_millis_identify += 1000;
    identifyTick();
    CHECK_EQ(bulb_send_off_calls, 3);
}

void testIdentifyBulbRestoreState() {
    check::begin("identifyBulb: restores true state");
    resetIdentifyTest();
    mock_bulb.state.power = true; // Initially ON

    bool result = identifyBulb(&mock_bulb);
    CHECK(result);
    // bulbSendOn is called initially
    CHECK_EQ(bulb_send_on_calls, 1);

    // 1st blink
    mock_millis_identify += 500; identifyTick(); // off
    mock_millis_identify += 400; identifyTick(); // on

    // 2nd blink
    mock_millis_identify += 500; identifyTick(); // off
    mock_millis_identify += 400; identifyTick(); // on

    // 3rd blink
    mock_millis_identify += 500; identifyTick(); // off

    // Done - restores state.
    // Last blink turned it off. So state.power is FALSE.
    // restorePower is TRUE.
    // state.power != restorePower is TRUE.
    // So it should call bulbSendOn!
    mock_millis_identify += 400; identifyTick();

    // Total ON calls:
    // 1 (initial) + 2 (blink 1,2) + 1 (restore) = 4
    CHECK_EQ(bulb_send_on_calls, 4);
}

void testIdentifyInvalidBulb() {
    check::begin("identifyBulb: invalid inputs");
    resetIdentifyTest();

    CHECK(!identifyBulb(nullptr));
    CHECK_EQ(bulb_send_on_calls, 0);

    mock_bulb_ready = false;
    CHECK(!identifyBulb(&mock_bulb));
    CHECK_EQ(bulb_send_on_calls, 0);
}

void testIdentifyBulbReplacement() {
    check::begin("identifyBulb: replace running sequence");
    resetIdentifyTest();
    mock_bulb.state.power = false;

    CHECK(identifyBulb(&mock_bulb)); // starts blink for bulb 1
    CHECK_EQ(bulb_send_on_calls, 1); // turns on
    // now mock_bulb.state.power = true

    // After a little bit, bulb 2 starts identifying
    mock_millis_identify += 100;
    CHECK(identifyBulb(&mock_bulb2)); // replaces blink

    // Bulb 1 had power=false originally, but is currently ON.
    // So stopCurrent -> bulbSendOff
    CHECK_EQ(bulb_send_off_calls, 1);

    // Bulb 2 is now blinking, it gets bulbSendOn immediately
    CHECK_EQ(bulb_send_on_calls, 2);

    // Tick advances bulb 2
    mock_millis_identify += 500;
    identifyTick(); // bulb 2 off
    CHECK_EQ(bulb_send_off_calls, 2);
}

void testIdentifyBulbRemoved() {
    check::begin("identifyBulb: removed during sequence");
    resetIdentifyTest();

    CHECK(identifyBulb(&mock_bulb));
    CHECK_EQ(bulb_send_on_calls, 1);

    mock_millis_identify += 500;
    identifyTick(); // turns off
    CHECK_EQ(bulb_send_off_calls, 1);

    // Bulb is deleted / goes unready
    mock_bulb_ready = false;

    mock_millis_identify += 400;
    identifyTick(); // recognizes removal, stops running

    CHECK_EQ(bulb_send_on_calls, 1); // unchanged
    CHECK_EQ(bulb_send_off_calls, 1); // unchanged
}

void runIdentifyTests() {
    testIdentifyBulbValid();
    testIdentifyBulbRestoreState();
    testIdentifyInvalidBulb();
    testIdentifyBulbReplacement();
    testIdentifyBulbRemoved();
}
