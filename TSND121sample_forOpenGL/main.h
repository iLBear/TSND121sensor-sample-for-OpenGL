//
//  main.h
//  TSND121sample_forOpenGL
//
//  Created by H.Sakuma on 2013/03/12.
//  Copyright (c) 2013年 H.Sakuma. All rights reserved.
//

#ifndef TSND121sample_forOpenGL_main_h
#define TSND121sample_forOpenGL_main_h

#include <iostream>
#include <math.h>
#include "TSND121.h"	//for using TSND121 sensor


int open_port();
void display();
void idle();
void myInit();
void keyboard();

int fd;					//ポートのファイルディスクリプタ

bool isMeasuring = false;	//計測中かどうか
struct termios options;

TSND121 TSND;

unsigned char buffer[64];
unsigned char *bufptr;
int nbytes;

std::string messageStr;

#endif
