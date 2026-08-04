#pragma once
#include <Arduino.h>

void Menu_Init();
void Menu_Open();
void Menu_Close();
void Menu_Next();
void Menu_Prev();
void Menu_Adjust(int delta);
int Menu_Selection();
