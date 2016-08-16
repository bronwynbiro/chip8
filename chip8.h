class chip8 {
	public:
		chip8();
		~chip8();

		bool drawFlag;

		void emulate();
		void render();
		bool load(const char * filename);

		unsigned char  graphics[2048];
		unsigned char  key[16];

	private:
		unsigned short programCounter;
		unsigned short opcode;
		unsigned short indexRegister;
		unsigned short stackPointer;

		unsigned char  V[16];			//(V0-VF)
		unsigned short stack[16];
		unsigned char  memory[4096];

		unsigned char  delayTimer;
		unsigned char  soundTimer;

		void init();
};
