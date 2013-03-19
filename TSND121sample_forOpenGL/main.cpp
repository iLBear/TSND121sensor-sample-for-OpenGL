//
//  main.cpp
//  TSND121sample_forOpenGL
//
//  Created by H.Sakuma on 2013/03/12.
//  Copyright (c) 2013年 H.Sakuma. All rights reserved.
//

#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include "main.h"
#include "inttypes.h"

const char *serialportname = "/dev/tty.TSND121_BT";

int open_port(){
    fd = open(serialportname, O_RDWR | O_NOCTTY | O_NDELAY);
    if(fd == -1){
        perror("open_port: unable to open port");
        exit(-1);
    }else{
        fcntl(fd, F_SETFL, 0);
    }
    return fd;
}

void myInit(){
	/* open port */
    open_port();
    printf("CONNECTED.\n\n");
    
    /* get current option of port */
    tcgetattr(fd, &options);
    
    /* set baud rate */
    cfsetispeed(&options, B57600);  //input speed
    cfsetospeed(&options, B57600);  //output speed
    
    /* enable receiver & set local mode */
    options.c_cflag     |= (CLOCAL | CREAD);
    options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag     &= ~OPOST;
    options.c_cc[VMIN]   = 0;
    options.c_cc[VTIME]  = 10;      //timeout 10ms
    
    /* set new option of port */
    tcsetattr(fd, TCSANOW, &options);
	
	/* 加速度設定を取得する為にメッセージを送信 */
	sleep(2);	//コマンドが高確率で届くように，接続してから数秒待つ
	TSND.command.getAccelMeasurement(fd);
}

void display(){
	
}

void idle(){
	bufptr = buffer;    //無駄な操作かもしれぬ
	//パケット読み取り．一気に送られてくるから1回呼び出しでOK
	nbytes = (int)read(fd, bufptr, buffer + sizeof(buffer) - bufptr - 1);
	
	//bufferに溜まった分(nbytes)だけループを回してメッセージ内容表示
	//        for(int i = 0; i < nbytes; i++){
	//            printf("%2d: %x\n", i, buffer[i]);
	//        }
	char str[10];
	char *ch;
	
	/* 以下メッセージの中身を解析 */
    if(nbytes > 0){
        switch(buffer[1]){
            case 0x80:  //加速度・角速度計測メッセージ
				for(int i = 0; i < 3; i++){
                    if(buffer[8+(3*i)] >= 0xF0){    //最上位4ビットが1ならマイナス（にした）
						sprintf(str, "0xff%02x%02x%02x", buffer[8+(3*i)], buffer[7+(3*i)], buffer[6+(3*i)]);
                    }else{
						sprintf(str, "0x00%02x%02x%02x", buffer[8+(3*i)], buffer[7+(3*i)], buffer[6+(3*i)]);
                    }
					TSND.accel[i] = (int)strtoimax(str, &ch, 16);	//requre [#include "inttypes.h"]
					TSND.rotate[i] = TSND.accel[i]*0.009;	//-10k~10kの値を取るらしいので,0.009を掛けることにより-90~90[deg]に変換した
                }
				
				
				//z軸方向の移動距離
				TSND.calcDistFromAccel('z');
				
				//              printf("X:0x%02x%02x%02x\n", buffer[8], buffer[7], buffer[6]);
				//				printf("Y:0x%02x%02x%02x\n", buffer[11], buffer[10], buffer[9]);
				//				printf("Z:0x%02x%02x%02x\n", buffer[14], buffer[13], buffer[12]);
				//              sprintf(str, "0x%02x%02x%02x", buffer[8], buffer[7], buffer[6]);
                printf("[acc]x:%3d, y:%3d, z;%3d\n", TSND.accel[0], TSND.accel[1], TSND.accel[2]);
				//				printf("[rot]x:%3f, y:%3f, z;%3f\n", TSND.rotate[0], TSND.rotate[1], TSND.rotate[2]);
				//				printf("            dist_z:%.3f, v0:%.3f\n", TSND.distance[2], TSND.velocity[2]);
				TSND.height_delta += TSND.distance[2];
				printf("height_delta:%f\n", TSND.height_delta);
                break;
                
            case 0x82:  //気圧計速メッセージ
                if(TSND.initializeP){
                    TSND.initPressure = 256*256*buffer[8]+256*buffer[7]+buffer[6];
                    TSND.initializeP = false;
                }
                printf("Msg[0x%02x%02x%02x]\n", buffer[8], buffer[7], buffer[6]);
                TSND.pressure = 256*256*buffer[8]+256*buffer[7]+buffer[6];
                
                TSND.height = 44330.77*(1.0-pow((TSND.pressure/101325.0), 0.1902632)); //海抜からの高度算出
                
                printf(" Pressure:%d[Pa] delta:%d\n", TSND.pressure, TSND.initPressure-TSND.pressure);
                printf(" Height:%.3f[m]\n", TSND.height);
                break;
				
			case 0x84:	//外部拡張端子データ通知
				//外部拡張入出力レベル（0:Low, 1:High）
				TSND.terminalIO[0] = buffer[6]&0x01;
				TSND.terminalIO[1] = (buffer[6]&0x02)>>1;
				TSND.terminalIO[2] = (buffer[6]&0x04)>>2;
				TSND.terminalIO[3] = (buffer[6]&0x08)>>3;
				
				//外部拡張端子AD値
				TSND.terminalAD[0] = 256*buffer[8]+buffer[7];
				TSND.terminalAD[1] = 256*buffer[10]+buffer[9];
				
				break;
				
            case 0x85:  //エッジ検出（オプションボタン押下など）
				//外部拡張エッジ検出有無（0:エッジ無し, 1:エッジ有り）
				TSND.terminalEdge[0] = buffer[6]&0x01;
				TSND.terminalEdge[1] = (buffer[6]&0x02)>>1;
				TSND.terminalEdge[2] = (buffer[6]&0x04)>>2;
				TSND.terminalEdge[3] = (buffer[6]&0x08)>>3;
				
				switch(buffer[7]){
					case 0x00:
						TSND.optionButton = 0;
						break;
						
					case 0x01:
						TSND.optionButton = 1;
						printf("Button pressed.\n");
						break;
						
					case 0x02:
						TSND.optionButton = 2;
						printf("Button released.\n");
						break;
						
					default:
						TSND.optionButton = 0;
						break;
				}
                break;
				
            case 0x87:  //計測エラー通知
                switch (buffer[6]) {
                    case 0x80:
                        messageStr = "Acceleration";
                        break;
                        
                    case 0x81:
                        messageStr = "Geomagnetism";
                        break;
                        
                    case 0x82:
                        messageStr = "Pressure";
                        break;
                        
                    case 0x86:
                        messageStr = "Extern I2C";
                        break;
                        
                    default:
                        messageStr = "UNDEFINED";
                        break;
                }
                printf("Error occured in measuring %s.\n", messageStr.c_str());
                break;
                
            case 0x88:  //計測開始通知
                printf("Start measuring.\n");
                break;
                
            case 0x89:  //計測終了通知
                isMeasuring = false;
                printf("Stop measuring.\n");
                break;
                
            case 0x8f:  //コマンドレスポンス
                if(buffer[2] == 0x00){
                    printf(" -> OK!\n");
                }else{  //0x01
                    printf(" -> NG\n");
                }
                break;
                
            case 0x92:  //現在時刻メッセージ
                printf("<%4d/%02d/%02d %02d:%02d:%02d>\n", buffer[2]+2000, buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
                break;
                
            case 0x93:  //計測時刻応答
				//                printf("stop measuring\n");
                break;
                
			case 0x97:
				TSND.setAccelCycle(buffer[2]);
				TSND.setAccelAverage(buffer[3]);
				printf("=== set accel Cy:%d, Av:%d\n", buffer[2], buffer[3]);
				break;
				
            case 0xbb:  //計測時刻応答
                printf("Battery %d%%\n", buffer[4]);
                break;
                
            default:
                printf("Got undefined command:%#02x\n", buffer[1]);
                break;
        }
    }
}



void keyboard(int key, int x, int y){
	switch(key){
		case 't':   //現在時刻取得
            TSND.command.getTime(fd);
            break;
            
        case 13:	//return key
            if(isMeasuring){    //計測停止
                isMeasuring = false;
                TSND.command.stopMeasure(fd);
            }else{              //計測開始
                TSND.initializeP = true;
                isMeasuring = true;
				TSND.height_delta = 0;
                TSND.command.startMeasure(fd, 60);
            }
            break;
            
        case ' ':
            break;
            
        case 'o':
            TSND.command.setOptionButtonMode(fd, 3);
            break;
            
        case 'b':
            TSND.command.getBatteryRemain(fd);
            break;
            
        case 'B':
            TSND.command.setBatteryMeasurement(fd, false, false);
            break;
            
        case 'a':   //加速度計測設定ON
            TSND.command.setAccelMeasurement(fd, 5, 10, 0);
			TSND.command.getAccelMeasurement(fd);
            break;
        case 'A':   //加速度計測設定OFF
            TSND.command.setAccelMeasurement(fd, 0, 10, 0);
            break;
            
        case 'g':   //地磁気計測設定ON
            TSND.command.setGeometricMeasurement(fd, 10, 10, 0);
            break;
        case 'G':   //地磁気計測設定OFF
            TSND.command.setGeometricMeasurement(fd, 0, 10, 0);
            break;
            
        case 'p':   //気圧計測設定ON
            TSND.initializeP = true;
            TSND.command.setPressureMeasurement(fd, 4, 5, 0);
            break;
        case 'P':   //気圧計測設定OFF
            TSND.initializeP = false;
            TSND.command.setPressureMeasurement(fd, 0, 5, 0);
            break;
			
		case 'i':	//外部拡張計測送信ON
			TSND.command.setIOMeasurement(fd, 5, 10, 0, true, false);
			break;
		case 'I':	//外部拡張計測送信OFF
			TSND.command.setIOMeasurement(fd, 0, 0, 0, false, false);
			break;
			
		case 'e':	//0x1E: 外部拡張端子計測＆エッジデータ出力設定
			TSND.command.setExternalIO(fd, 0, 0, 0, 0);
			break;
			
		case 'r':
			TSND.command.setAccelRange(fd, 0);
			break;
			
		case 'c':
			TSND.command.collectAccelMeasurement(fd, 1, 1, 1, 0, 0, 0);
			break;
			
		case 'z':
			TSND.command.setBuzzerVolume(fd, 1);
			break;
			
		case '1':
			TSND.command.playBuzzer(fd, 1);
			break;
		case '2':
			TSND.command.playBuzzer(fd, 2);
			break;
		case '3':
			TSND.command.playBuzzer(fd, 3);
			break;
		case '4':
			TSND.command.playBuzzer(fd, 4);
			break;
		case '5':
			TSND.command.playBuzzer(fd, 5);
			break;
		case '6':
			TSND.command.playBuzzer(fd, 6);
			break;
		case '7':
			TSND.command.playBuzzer(fd, 7);
			break;
		case '0':
			TSND.command.playBuzzer(fd, 0);
			break;
			
		case 27:	//esc key
			break;
			
		default:
			break;
	}
}

int main(int argc, char * argv[])
{
	//Initialize
	glutInit(&argc, argv);
	myInit();
	
	glutInitWindowSize(640, 640);
	glutCreateWindow("TSND121 sample");
	
	glutIdleFunc(idle);
	glutDisplayFunc(display);
	glutSpecialFunc(keyboard);
	
	glutMainLoop();
	
    return 0;
}

