#include "check.h"
#include "shim/WString.h"
#include "../credentials_logic.h"
#include "../update_check.h"

void runUpdateCheckTests() {
  check::begin("update check");

  int v[3];
  CHECK(parseVersion(String("1.2.3"), v) && v[0] == 1 && v[1] == 2 && v[2] == 3);
  CHECK(parseVersion(String("v10.0.21"), v) && v[0] == 10 && v[2] == 21);
  CHECK(!parseVersion(String("1.2"), v));
  CHECK(!parseVersion(String("1.2.3.4"), v));
  CHECK(!parseVersion(String("1.2.3-beta"), v));
  CHECK(!parseVersion(String("v"), v));
  CHECK(!parseVersion(String(""), v));
  CHECK(!parseVersion(String("1..3"), v));
  CHECK(!parseVersion(String("a.b.c"), v));

  CHECK(versionNewer(String("1.1.0"), String("1.2.0")));
  CHECK(versionNewer(String("1.1.0"), String("v1.1.1")));
  CHECK(versionNewer(String("1.9.9"), String("2.0.0")));
  CHECK(versionNewer(String("1.2.0"), String("1.10.0")));  // Numeric, not textual.
  CHECK(!versionNewer(String("1.2.0"), String("1.2.0")));
  CHECK(!versionNewer(String("1.2.0"), String("v1.1.9")));
  CHECK(!versionNewer(String("1.2.0"), String("junk")));
  CHECK(!versionNewer(String("junk"), String("1.2.0")));
  CHECK(sameVersion(String("1.2.0"), String("v1.2.0")));
  CHECK(!sameVersion(String("1.2.0"), String("v1.2.1")));

  const char *repo = "MattiaPell/esp32-zigbee-bulb";
  String tag;
  CHECK(tagFromLocation(String("https://github.com/MattiaPell/esp32-zigbee-bulb/releases/tag/v1.2.0"), repo, tag));
  CHECK_EQ(tag, String("v1.2.0"));
  CHECK(tagFromLocation(String("/mattiapell/esp32-zigbee-bulb/releases/tag/v1.3.4"), repo, tag));
  CHECK_EQ(tag, String("v1.3.4"));
  CHECK(tagFromLocation(String("https://github.com/MattiaPell/esp32-zigbee-bulb/releases/tag/v2.0.0?x=1"), repo, tag));
  CHECK_EQ(tag, String("v2.0.0"));
  // Wrong host, wrong repo, odd tags and empty input are all refused.
  CHECK(!tagFromLocation(String("https://evil.example.com/MattiaPell/esp32-zigbee-bulb/releases/tag/v1.2.0"), repo, tag));
  CHECK(!tagFromLocation(String("https://github.com.evil.com/MattiaPell/esp32-zigbee-bulb/releases/tag/v1.2.0"), repo, tag));
  CHECK(!tagFromLocation(String("https://github.com/Other/repo/releases/tag/v1.2.0"), repo, tag));
  CHECK(!tagFromLocation(String("https://github.com/MattiaPell/esp32-zigbee-bulb/releases/tag/v1.2.0/../../x"), repo, tag));
  CHECK(!tagFromLocation(String("https://github.com/MattiaPell/esp32-zigbee-bulb/releases/tag/nightly"), repo, tag));
  CHECK(!tagFromLocation(String("https://github.com/MattiaPell/esp32-zigbee-bulb/releases"), repo, tag));
  CHECK(!tagFromLocation(String(""), repo, tag));

  CHECK_EQ(releaseAssetUrl(repo, String("v1.2.0"), "fw.bin"),
           String("https://github.com/MattiaPell/esp32-zigbee-bulb/releases/download/v1.2.0/fw.bin"));

  uint8_t hash[32];
  const String hex("00ff10AB00ff10ab00ff10ab00ff10ab00ff10ab00ff10ab00ff10ab00ff10ab");
  CHECK(parseSha256Hex(hex, hash));
  CHECK(hash[0] == 0x00 && hash[1] == 0xff && hash[2] == 0x10 && hash[3] == 0xab);
  CHECK(parseSha256Hex(String("\n") + hex + String("  esp32-zigbee-bulb.bin\n"), hash));
  CHECK(!parseSha256Hex(hex.substring(0, 63), hash));
  CHECK(!parseSha256Hex(hex + String("0"), hash));  // 65 digits is not a hash.
  CHECK(!parseSha256Hex(String("zz") + hex.substring(2), hash));
  CHECK(!parseSha256Hex(String(""), hash));

  uint8_t a[32] = {1}, b[32] = {1};
  CHECK(sha256Equal(a, b));
  b[31] = 1;
  CHECK(!sha256Equal(a, b));

  // Credentials: a real compiled value wins, placeholders never do.
  CHECK(credIsPlaceholder("your-wifi-name"));
  CHECK(credIsPlaceholder("your-ota-secret"));
  CHECK(credIsPlaceholder(""));
  CHECK(!credIsPlaceholder("HomeNet"));
  WifiCreds w = credResolveWifi("HomeNet", "pw", String("old"), String("oldpw"));
  CHECK_EQ(w.ssid, String("HomeNet"));
  CHECK_EQ(w.pass, String("pw"));
  w = credResolveWifi("HomeNet", "", String("old"), String("oldpw"));  // Open network.
  CHECK_EQ(w.pass, String(""));
  w = credResolveWifi("your-wifi-name", "your-wifi-password", String("Stored"), String("spw"));
  CHECK_EQ(w.ssid, String("Stored"));
  CHECK_EQ(w.pass, String("spw"));
  CHECK(credWifiNeedsSeed("HomeNet", "pw", String(""), String("")));
  CHECK(credWifiNeedsSeed("HomeNet", "pw2", String("HomeNet"), String("pw")));
  CHECK(!credWifiNeedsSeed("HomeNet", "pw", String("HomeNet"), String("pw")));
  CHECK(!credWifiNeedsSeed("your-wifi-name", "x", String("Stored"), String("spw")));
  CHECK_EQ(credResolveToken("real", String("old")), String("real"));
  CHECK_EQ(credResolveToken("your-ota-secret", String("kept")), String("kept"));
  CHECK(credTokenNeedsSeed("real", String("old")));
  CHECK(!credTokenNeedsSeed("real", String("real")));
  CHECK(!credTokenNeedsSeed("your-ota-secret", String("kept")));
  CHECK(credTokenUsable(String("kept")));
  CHECK(!credTokenUsable(String("")));
  CHECK(!credTokenUsable(String("your-ota-secret")));
}
