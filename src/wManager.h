#ifndef _WMANAGER_H
#define _WMANAGER_H

void init_WifiManager();
void wifiManagerProcess();
extern bool calibrationInProgress;
extern bool displayPaused;
void reset_configuration();

#endif // _WMANAGER_H
