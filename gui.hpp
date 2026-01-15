#ifndef GUI_H
#define GUI_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <string>
#include <proto/asl.h>
#include "Common.h"

// Gadget IDs
#define ID_SEEKER 1
#define ID_PLAY   2
#define ID_STOP   3
#define ID_OPEN   4

#define ID_TIME_DISPLAY 1004

// Funktions-Prototypen
bool setupGUI();
void cleanupGUI();
void updateSeeker(long percent);
void updateTimeDisplay(uint32_t lap, uint32_t total);
void drawVideoPlaceholder();
std::string openFileRequest();

// Globale Pointer, damit main.cpp darauf zugreifen kann
extern struct Window *win;
extern struct Gadget *gList;

#endif