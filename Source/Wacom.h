#pragma once

#include <unordered_map>
#include <SDKDDKVer.h>
#include <windows.h>

#include <WacomMultiTouch.h>

#define MAX_ATTACHED_DEVICES 10

namespace Wacom {

using FingerId = int;

enum class TouchState {
    None = 0, Up, Down, Hold
};

struct TouchEvent {
    FingerId fingerId;
    int fingerCount;

    // Whether or not this can be considered a finger,
    // or erroneous input such as your palm or elbow
    bool confidence;

    TouchState state;

    // Normalised coordinate between 0-1
    float x;
    float y;

    // Width of the contact area
    // Appears to fall within 5-6 unique values, i.e. low precision
    float height;
    float width;

    // Angle of contact point, related to the Cintiq
    float orientation;

    unsigned short sensitivity;
};

// Keep track of last spotted events, for `poll()`
using PollEvents = std::unordered_map<int, TouchEvent>;


class Touch {
public:
    bool init();
    void printAttachedDevices() const;

    // Either poll for events..
    auto poll() -> const PollEvents;

    // ..or attach a callback (or both)
    //
    // Depending on your polling rate, these may be more frequent
    // and carry less duplicate data.
    virtual void touchDownEvent(TouchEvent event) {}
    virtual void touchUpEvent(TouchEvent event) {}
    virtual void touchHoldEvent(TouchEvent event) {}

    // Internal callbacks for Wacom
    void _deviceAttached(WacomMTCapability deviceInfo);
    void _deviceDetached(int deviceID);
    void _fingerCallBack(WacomMTFingerCollection *fingerPacket);

private:
    PollEvents _events;

    void _touchDownEvent(TouchEvent event);
    void _touchUpEvent(TouchEvent event);
    void _touchHoldEvent(TouchEvent event);

};

}