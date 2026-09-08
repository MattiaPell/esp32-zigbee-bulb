#pragma once

// Minimal serial console for headless setup and debugging.
// Commands: help, list, on/off/all <n>, pair, host <name>, status, reboot, reset
void serialConsoleBegin();
void serialConsoleTick();
