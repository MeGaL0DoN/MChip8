#include <bitset>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <random>
#include "MiniAudio/miniaudio.h"
#include "Quirks.h"

class ChipCore
{
public:
	static constexpr int SCRWidth = 64;
	static constexpr int SCRHeight = 32;

	int CPUfrequency { 500 };
	bool enableSound { true };

	ChipCore()
	{
		initialize();
		ma_engine_init(NULL, &soundEngine);
		ma_engine_set_volume(&soundEngine, 0.1f);
	}

	inline bool getPixel(uint8_t x, uint8_t y)
	{
		return screenBuffer[SCRWidth * y + x];
	}
	inline void setKey(uint8_t key, bool isPressed)
	{
		keys[key] = isPressed;
		if (inputReg != nullptr && !isPressed)
		{
			*inputReg = key;
			inputReg = nullptr;
		}
	}

	void loadROM(std::string_view path)
	{
		initialize();

		std::ifstream ifs(path.data(), std::ios::binary | std::ios::ate);
		std::ifstream::pos_type pos = ifs.tellg();

		ifs.seekg(0, std::ios::beg);
		ifs.read(reinterpret_cast<char*>(&RAM[0x200]), pos);
		ifs.close();
	}

	void updateTimers()
	{
		if (delay_timer > 0) delay_timer--;
		if (sound_timer > 0)
		{
			sound_timer--;
			if (enableSound) beep();
		}
	}

	void emulateCycle()
	{
		if (inputReg != nullptr) return;

		const uint16_t opcode = (RAM[pc] << 8) | RAM[pc + 1];
		bool incrementCounter { true };

		uint8_t xOperand = (opcode & 0x0F00) >> 8;
		uint8_t& regX = V[xOperand];
		const uint8_t regY = V[(opcode & 0x00F0) >> 4];

		const uint8_t doubleNibble = opcode & 0x00FF;
		const uint16_t memoryAddr = opcode & 0x0FFF;

		switch (opcode & 0xF000)
		{
		case 0x0000:
		{
			switch (opcode & 0x000F)
			{
			case 0x0000: 
				clearScreen();
				break;
			case 0x000E: 
				pc = stack[--sp];
				break;
			}
			break;
		}
		case 0x1000: 
			pc = memoryAddr;
			incrementCounter = false;
			break;  
		case 0x2000:
			stack[sp++] = pc;
			pc = memoryAddr;
			incrementCounter = false;
			break;
		case 0x3000:
			if (regX == doubleNibble) skipNextInstr();
			break;
		case 0x4000:
			if (regX != doubleNibble) skipNextInstr();
			break;
		case 0x5000:
			if (regX == regY) skipNextInstr();
			break;
		case 0x6000:
			regX = doubleNibble;
			break;
		case 0x7000: 
			regX += doubleNibble;
			break;
		case 0x8000:
			switch (opcode & 0x000F)
			{
			case 0x0000:
				regX = regY;
				break;
			case 0x0001:
				regX |= regY;
				if (Quirks::VFReset) V[0xF] = 0;
				break;
			case 0x0002:
				regX &= regY;
				if (Quirks::VFReset) V[0xF] = 0;
				break;
			case 0x0003:
				regX ^= regY;
				if (Quirks::VFReset) V[0xF] = 0;
				break;
			case 0x0004:
			{
				int result = regX + regY;
				regX = result;
				V[0xF] = result > 255;
				break;
			}
			case 0x0005:
			{
				int result = regX - regY;
				regX = result;
				V[0xF] = result >= 0;
				break;
			}
			case 0x0006: 
			{
			    if (!Quirks::Shifting) regX = regY;
				uint8_t lsb = regX & 1;
				regX >>= 1;
				V[0xF] = lsb;
				break;
			}
			case 0x0007: 
				regX = regY - regX;
				V[0xF] = regY >= regX;
				break;
			case 0x000E: 
			{
				if (!Quirks::Shifting) regX = regY;
				uint8_t msb = (regX & 0x80) >> 7;
				regX <<= 1;
				V[0xF] = msb;
				break;
			}
			}
			break;
		case 0x9000:
			if (regX != regY) skipNextInstr();
			break;
		case 0xA000:
			I = memoryAddr;
			break;
		case 0xB000:
			if (Quirks::Jumping) pc = regX + memoryAddr;
			else pc = V[0] + memoryAddr;
			incrementCounter = false;
			break;
		case 0xC000:
			regX = rngDistr(rngEng) & doubleNibble;
			break;
		case 0xD000: 
			drawSprite(regX % SCRWidth, regY % SCRHeight, opcode & 0x000F);
			break;
		case 0xE000:
			switch (opcode & 0x000F)
			{
			case 0x000E:
				if (keys[regX]) skipNextInstr();
				break;
			case 0x0001:
				if (!keys[regX]) skipNextInstr();
				break;
			}
			break;
		case 0xF000:
			switch (opcode & 0x00FF)
			{
			case 0x0007:
				regX = delay_timer;
				break;
			case 0x000A: 
				inputReg = &regX;
				break;
			case 0x001E:
				I += regX;
				break;
			case 0x0015:
				delay_timer = regX;
				break;
			case 0x0018:
				sound_timer = regX;
				break;
			case 0x0029:
				I = regX * 0x5;
				break;
			case 0x0033:
				RAM[I] = regX / 100;
				RAM[I + 1] = (regX / 10) % 10;
				RAM[I + 2] = (regX % 100) % 10;
				break;
			case 0x0055:
				for (int i = 0; i <= xOperand; i++)
					RAM[I + i] = V[i];
				if (Quirks::MemoryIncrement) I += xOperand + 1;
				break;
			case 0x0065:
				for (int i = 0; i <= xOperand; i++) 
					V[i] = RAM[I + i];
				if (Quirks::MemoryIncrement) I += xOperand + 1;
				break;
			}
			break;
		}

		if (incrementCounter)
			pc += 2;
	}

private:
	std::bitset<SCRWidth * SCRHeight> screenBuffer{};
	uint8_t RAM[4096];

	uint8_t V[16];
	uint16_t I;
	uint16_t pc;

	uint8_t delay_timer;
	uint8_t sound_timer;

	uint16_t stack[16];
	uint16_t sp;

	std::bitset<16> keys{};
	uint8_t* inputReg;

	std::default_random_engine rngEng { std::random_device{}() };
	std::uniform_int_distribution<> rngDistr { 0, 255 };

	static constexpr uint8_t fontset[80] =
	{
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};

	ma_engine soundEngine;
	inline void beep()
	{
		ma_engine_play_sound(&soundEngine, "data/beep.wav", NULL);
	}

	inline void clearScreen()
	{
		screenBuffer.reset();
	}
	inline void setPixel(uint8_t x, uint8_t y, bool val)
	{
		screenBuffer[SCRWidth * y + x] = val;
	}

	inline void drawSprite(uint8_t Xpos, uint8_t Ypos, uint8_t height)
	{
		constexpr uint8_t width = 8;
		V[0xF] = 0; 

		for (int i = 0; i < height; i++)
		{
			uint8_t spriteRow = RAM[I + i];
			uint8_t screenY = i + Ypos;

			if (Quirks::Clipping)
			{
				if (screenY >= SCRHeight)
					return;
			}
			else
				screenY %= SCRHeight;

			for (int j = 0; j < width; j++)
			{
				bool spritePixel = spriteRow & (0x80 >> j);
				if (spritePixel)
				{
					uint8_t screenX = j + Xpos;

					if (Quirks::Clipping)
					{
						if (screenX >= SCRWidth)
							return;
					}
					else
						screenX %= SCRWidth;

					bool screenPixel = getPixel(screenX, screenY);
					if (screenPixel) V[0xF] = 1;
					setPixel(screenX, screenY, !screenPixel);
				}
			}
		}
	}

	constexpr void skipNextInstr() { pc += 2; }

	void initialize()
	{
		pc = 0x200;  
		I = 0;      
		sp = 0;
		delay_timer = 0;
		sound_timer = 0;

		std::memset(V, 0, sizeof(V));
		std::memset(RAM, 0, sizeof(RAM));
		std::memcpy(RAM, fontset, sizeof(fontset));

		clearScreen();
		keys.reset();
		inputReg = nullptr;
	}
};