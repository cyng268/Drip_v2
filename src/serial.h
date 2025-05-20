#ifndef SERIAL_H
#define SERIAL_H

#include "common.h"
#include "serialib.h"

extern serialib cameraSerial;
extern bool serialInitialized;

// Initialize serial connection
bool initializeSerial();

// Send zoom command
void sendZoomCommand(int level);

// Function to convert decimal to hex string
std::string decToHex(int decimalNumber);

// Zoom functions
void zoomIn();
void zoomOut();
void sendICRCommand(bool enable);
void sendIRCorrectionCommand(bool enable);
void sendStabilizerCommand(bool enable);

#endif // SERIAL_H
