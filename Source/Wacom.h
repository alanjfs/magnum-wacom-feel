#pragma once

#include <SDKDDKVer.h>
#include <windows.h>

#include <WacomMultiTouch.h>

#define MAX_ATTACHED_DEVICES 10

namespace Wacom {

struct TouchEvent {
    int id;
    int time;

    float x;
    float y;

    float height;
    float width;
    float angle;

    unsigned short sensitivity;
    bool confidence;

    int numTouches;

    float xaccel;
    float xspeed;
    float yaccel;
    float yspeed;
};


class Tablet {
public:
    bool init();
    void listAttachedDevices();

    virtual void TouchDownEvent(TouchEvent event) {}
    virtual void TouchUpEvent(TouchEvent event) {}
    virtual void TouchHoldEvent(TouchEvent event) {}

    void _deviceAttached(WacomMTCapability deviceInfo);
    void _deviceDetached(int deviceID);
    void _fingerCallBack(WacomMTFingerCollection *fingerPacket);
};

}