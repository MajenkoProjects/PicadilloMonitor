#include <Picadillo.h>

Picadillo tft;

typedef enum {
	IDLE = 0,
	SOF,
	SOH,
	IDHIGH,
	IDLOW,
	EOH,
	DATA,
	EOT	,
	DONE
} Mode;

void setup() {
	tft.initializeDevice();
	tft.setBacklight(255);
	tft.setRotation(1);
	tft.fillScreen(Color::Black);
	Serial.begin(2000000);
}

void loop() {
	static Mode mode = IDLE;
	static uint8_t data[128] = {0};
	static uint16_t blockno = 0;
	static uint8_t byteno = 0;
	
	if (Serial.available()) {
		int c = Serial.read();
		switch (mode) {
			case IDLE:
				mode = (c == 0x16) ? SOF : IDLE;
				break;
			case SOF:
				mode = (c == 0x16) ? SOH : IDLE;
				break;
			case SOH:
				mode = (c == 0x01) ? IDHIGH : IDLE;
				break;
			case IDHIGH:
				blockno = (c & 0xFF) << 8;
				mode = IDLOW;
				break;
			case IDLOW:
				blockno |= (c & 0xFF);
				mode = EOH;
				break;
			case EOH:
				mode = (c == 0x02) ? DATA : IDLE;
				byteno = 0;
				break;
			case DATA:
				data[byteno++] = c;
				if (byteno == 128) {
					mode = EOT;
				}
				break;
			case EOT:
				mode = (c == 0x04) ? IDLE : IDLE;
				renderBlock(blockno, (uint16_t *)data);
				break;
		}
	}
}

void renderBlock(uint16_t blockno, uint16_t *data) {
	int x = (blockno % 60) * 8;
	int y = (blockno / 60) * 8;

	tft.openWindow(x, y, 8, 8);
	tft.windowData(data, 64);
	tft.closeWindow();
}
