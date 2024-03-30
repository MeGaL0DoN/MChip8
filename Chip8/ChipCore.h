#include <bitset>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <random>
#include "MiniAudio/miniaudio.h"
#include "Quirks.h"

struct soundData
{
	ma_waveform* waveForm;
	uint8_t& soundTimer;
	bool& enableSound;
};

inline void sound_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	soundData* data = (soundData*)pDevice->pUserData;
	if (data->waveForm == nullptr)
		return;

	if (data->enableSound && data->soundTimer > 0)
	{
		ma_waveform_read_pcm_frames(data->waveForm, pOutput, frameCount, nullptr);
	}
}

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
		initAudio();
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
	void setVolume(double val)
	{
		ma_waveform_set_amplitude(&waveform, val);
	}

	void loadROM(const wchar_t* path)
	{
		initialize();

		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		std::ifstream::pos_type pos = ifs.tellg();

		if (pos <= sizeof(RAM) - 0x200)
		{
			ifs.seekg(0, std::ios::beg);
			ifs.read(reinterpret_cast<char*>(&RAM[0x200]), pos);
		}

		ifs.close();
	}

	void updateTimers()
	{
		if (delay_timer > 0) delay_timer--;
		if (sound_timer > 0) sound_timer--;
	}

	void emulateCycle()
	{
		if (inputReg != nullptr) return;

		const uint16_t opcode = (RAM[pc & 0xFFF] << 8) | RAM[(pc + 1) & 0xFFF];
		bool incrementCounter { true };

		const uint8_t xOperand = (opcode & 0x0F00) >> 8;
		const uint8_t yOperand = (opcode & 0x00F0) >> 4;

		uint8_t& regX = V[xOperand & 0xF];
		const uint8_t regY = V[yOperand & 0xF];

		const uint8_t doubleNibble = opcode & 0x00FF;
		const uint16_t memoryAddr = opcode & 0x0FFF;

		switch (opcode & 0xF000)
		{
		case 0x0000:
		{
			switch (opcode & 0x0FFF)
			{
			case 0x00E0: 
				clearScreen();
				break;
			case 0x00EE: 
				pc = stack[(--sp) & 0xF];
				break;
			}
			break;
		}
		case 0x1000: 
			pc = memoryAddr;
			incrementCounter = false;
			break;  
		case 0x2000:
			stack[(sp++) & 0xF] = pc;
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
			switch (opcode & 0x000F)
			{
			case 0:
				if (regX == regY) skipNextInstr();
				break;
			}
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
			switch (opcode & 0x000F)
			{
			case 0:
				if (regX != regY) skipNextInstr();
				break;
			}
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
			switch (opcode & 0x00FF)
			{
			case 0x009E:
				if (keys[regX & 0xF]) skipNextInstr();
				break;
			case 0x00A1:
				if (!keys[regX & 0xF]) skipNextInstr();
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
				I = (regX & 0xF) * 0x5;
				break;
			case 0x0033:
				RAM[I & 0xFFF] = regX / 100;
				RAM[(I + 1) & 0xFFF] = (regX / 10) % 10;
				RAM[(I + 2) & 0xFFF] = (regX % 100) % 10;
				break;
			case 0x0055:
				for (int i = 0; i <= (xOperand & 0xF); i++)
					RAM[(I + i) & 0xFFF] = V[i];

				if (Quirks::MemoryIncrement) I += xOperand + 1;
				break;
			case 0x0065:
				for (int i = 0; i <= (xOperand & 0xF); i++) 
					V[i] = RAM[(I + i) & 0xFFF];

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

	ma_device soundDevice;
	ma_waveform waveform;
	soundData sound_data { &waveform, sound_timer, enableSound };

	void initAudio()
	{
		constexpr double initialVolume = 0.5;
		constexpr int frequency = 440;

		ma_waveform_config config;
		ma_device_config deviceConfig;

		config = ma_waveform_config_init(ma_format_f32, 2, 44100, ma_waveform_type_square, initialVolume, frequency);
		ma_waveform_init(&config, &waveform);

		deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = 2;
		deviceConfig.sampleRate = 44100;
		deviceConfig.dataCallback = sound_data_callback;
		deviceConfig.pUserData = &sound_data;

		ma_device_init(NULL, &deviceConfig, &soundDevice);
		ma_device_start(&soundDevice);
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
					continue;
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
							continue;
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