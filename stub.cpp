/*
 * Fade To Black engine rewrite
 * Copyright (C) 2006-2012 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <getopt.h>
#include "file.h"
#include "game.h"
#include "mixer.h"
#include "sound.h"
#include "render.h"
#include "stub.h"

extern char home_directory[128];

static const char *USAGE =
	"Fade2Black/OpenGL\n"
	"Usage: f2b [OPTIONS]...\n"
	"  --datapath=PATH             Path to data files (default '.')\n"
	"  --language=EN|FR|GR|SP|IT   Language files to use (default 'EN')\n"
	"  --playdemo                  Use inputs from .DEM files\n"
	"  --level=NUM                 Start at level NUM\n"
	"  --voice=EN|FR|GR            Voice files (default 'EN')\n"
	"  --subtitles                 Display cutscene subtitles\n"
	"  --savepath=PATH             Path to save files (default '.')\n";

static const struct {
	FileLanguage lang;
	const char *str;
	bool voice;
} _languages[] = {
	{ kFileLanguage_EN, "EN", true  },
	{ kFileLanguage_FR, "FR", true  },
	{ kFileLanguage_GR, "GR", true  },
	{ kFileLanguage_SP, "SP", false },
	{ kFileLanguage_IT, "IT", false }
};

static FileLanguage parseLanguage(const char *language) {
	for (unsigned int i = 0; i < ARRAYSIZE(_languages); ++i) {
		if (strcasecmp(_languages[i].str, language) == 0) {
			return _languages[i].lang;
		}
	}
	return _languages[0].lang;
}

static FileLanguage parseVoice(const char *voice, FileLanguage lang) {
	switch (lang) {
	case kFileLanguage_SP:
	case kFileLanguage_IT:
		for (unsigned int i = 0; i < ARRAYSIZE(_languages); ++i) {
			if (strcasecmp(_languages[i].str, voice) == 0) {
				if (_languages[i].voice) {
					return _languages[i].lang;
				}
				break;
			}
		}
		// default to English
		return kFileLanguage_EN;
	default:
		// voice must match text for other languages
		return lang;
	}
}

static int getNextIntroCutsceneNum(int num) {
	switch (num) {
	case 47:
		return 39;
	case 39:
		return 13;
	case 13:
		return 37;
	case 37:
		return 53;
	case 53:
		return 29;
	}
	return -1;
}

static char *_dataPath;
static char *_savePath;
static bool _skipCutscenes;

struct GameStub_F2B : GameStub {

	enum {
		kStateCutscene,
		kStateGame,
		kStateInventory,
		kStateBox,
		kStateMenu,
	};

	Render *_render;
	Game *_g;
	int _state, _nextState;
	int _dt;
	bool _skip;
	int _slotState;
	bool _loadState, _saveState;

	void setState(int state) {
		debug(kDebug_INFO, "stub.state %d", state);
		// release
		if (_state == kStateCutscene) {
			_render->setPalette(_g->_screenPalette, 256);
		}
		// init
		if (state == kStateCutscene) {
			if (_skipCutscenes) {
				warning("Skipping cutscene %d playback", _g->_cut._numToPlay);
				_g->_cut._numToPlay = -1;
				return;
			}
			_g->_cut.load(_g->_cut._numToPlay);
		}
		if (state == kStateGame) {
			_render->setOverlayDim(0, 0);
			_g->updatePalette();
		}
		if (state == kStateInventory) {
			_render->setOverlayDim(320, 200);
			if (!_g->initInventory()) {
				return;
			}
		}
		if (state == kStateBox) {
			_render->setOverlayDim(320, 200);
			_g->initBox();
		}
		if (state == kStateMenu) {
			_g->initMenu();
		}
		_state = state;
	}

	virtual int init(int argc, char *argv[]) {
		GameParams params;
		char *language = 0;
		char *voice = 0;
		while (1) {
			static struct option options[] = {
				{ "datapath", required_argument, 0, 1 },
				{ "language", required_argument, 0, 2 },
				{ "playdemo", no_argument,       0, 3 },
				{ "level",    required_argument, 0, 4 },
				{ "voice",    required_argument, 0, 5 },
				{ "subtitles", no_argument,      0, 6 },
				{ "savepath", required_argument, 0, 7 },
#ifdef F2B_DEBUG
				{ "xpos_conrad",    required_argument, 0, 100 },
				{ "zpos_conrad",    required_argument, 0, 101 },
				{ "skip_cutscenes", no_argument,       0, 102 },
#endif
				{ 0, 0, 0, 0 }
			};
			int index;
			const int c = getopt_long(argc, argv, "", options, &index);
			if (c == -1) {
				break;
			}
			switch (c) {
			case 1:
				_dataPath = strdup(optarg);
				break;
			case 2:
				language = strdup(optarg);
				break;
			case 3:
				params.playDemo = true;
				break;
			case 4:
				params.levelNum = atoi(optarg);
				break;
			case 5:
				voice = strdup(optarg);
				break;
			case 6:
				params.subtitles = true;
				break;
			case 7:
				_savePath = strdup(optarg);
				break;
#ifdef F2B_DEBUG
			case 100:
				params.xPosConrad = atoi(optarg);
				break;
			case 101:
				params.zPosConrad = atoi(optarg);
				break;
			case 102:
				_skipCutscenes = true;
				break;
#endif
			default:
				printf("%s\n", USAGE);
				return -1;
			}
		}
		g_utilDebugMask = kDebug_INFO;
#ifdef F2B_DEBUG
		g_utilDebugMask |= kDebug_GAME /* | kDebug_RESOURCE */ | kDebug_FILE | kDebug_CUTSCENE | kDebug_OPCODES | kDebug_SOUND;
		_skipCutscenes = 1;
#endif
		FileLanguage fileLanguage = language ? parseLanguage(language) : kFileLanguage_EN;
		FileLanguage fileVoice = voice ? parseVoice(voice, fileLanguage) : fileLanguage;
		free(language);
		language = 0;
		free(voice);
		voice = 0;
		//if (!fileInit(fileLanguage, fileVoice, _dataPath ? _dataPath : ".", _savePath ? _savePath : ".")) {
		if (!fileInit(fileLanguage, fileVoice, home_directory, home_directory)) 
		{
			if (!fileInit(fileLanguage, fileVoice, ".", home_directory)) {
			warning("Unable to find datafiles");
			return -2;
			}
		}
		_render = new Render;
		_g = new Game(_render, &params);
		_g->init();
		_g->_cut._numToPlay = 47;
		_state = -1;
		_nextState = _skipCutscenes ? kStateGame : kStateCutscene;
		setState(_nextState);
		_nextState = _state;
		_dt = 0;
		_skip = false;
		_slotState = 0;
		_loadState = _saveState = false;
		return 0;
	}
	virtual void quit() {
		delete _g;
		delete _render;
		free(_dataPath);
		_dataPath = 0;
	}
	virtual StubMixProc getMixProc(int rate, int fmt, void (*lock)(int)) {
		StubMixProc mix;
		mix.proc = Mixer::mixCb;
		mix.data = &_g->_snd._mix;
		_g->_snd._mix.setFormat(rate, fmt);
		_g->_snd._mix._lock = lock;
		return mix;
	}
	virtual void queueKeyInput(int keycode, int pressed) {
		switch (keycode) {
		case kKeyCodeLeft:
			if (pressed) _g->inp.dirMask |= kInputDirLeft;
			else _g->inp.dirMask &= ~kInputDirLeft;
			break;
		case kKeyCodeRight:
			if (pressed) _g->inp.dirMask |= kInputDirRight;
			else _g->inp.dirMask &= ~kInputDirRight;
			break;
		case kKeyCodeUp:
			if (pressed) _g->inp.dirMask |= kInputDirUp;
			else _g->inp.dirMask &= ~kInputDirUp;
			break;
		case kKeyCodeDown:
			if (pressed) _g->inp.dirMask |= kInputDirDown;
			else _g->inp.dirMask &= ~kInputDirDown;
			break;
		case kKeyCodeAlt:
			_g->inp.altKey = pressed;
			break;
		case kKeyCodeShift:
			_g->inp.shiftKey = pressed;
			break;
		case kKeyCodeCtrl:
			_g->inp.ctrlKey = pressed;
			break;
		case kKeyCodeSpace:
			_g->inp.spaceKey = pressed;
			break;
		case kKeyCodeTab:
			_g->inp.tabKey = pressed;
			break;
		case kKeyCodeEscape:
			_g->inp.escapeKey = pressed;
			break;
		case kKeyCodeI:
			_g->inp.inventoryKey = pressed;
			break;
		case kKeyCodeJ:
			_g->inp.jumpKey = pressed;
			break;
		case kKeyCodeU:
			_g->inp.useKey = pressed;
			break;
		case kKeyCodeReturn:
			_g->inp.enterKey = pressed;
			break;
		case kKeyCode1:
		case kKeyCode2:
		case kKeyCode3:
		case kKeyCode4:
		case kKeyCode5:
			_g->inp.numKeys[1 + keycode - kKeyCode1] = pressed;
			break;
		case kKeyCodeCheatLifeCounter:
			_g->_cheats ^= kCheatLifeCounter;
			break;
		}
	}
	bool syncTicks(unsigned int ticks, int tickDuration) {
		static int previousTicks = ticks;
		_dt += ticks - previousTicks;
		const bool ret = (_dt < tickDuration);
		previousTicks = ticks;
		while (_dt >= tickDuration) {
			_dt -= tickDuration;
		}
		return ret;
	}
	virtual void doTick(unsigned int ticks) {
		if (_nextState != _state) {
			setState(_nextState);
		}
		_nextState = _state;
		switch (_state) {
		case kStateCutscene:
			_render->clearScreen();
			_skip = syncTicks(ticks, kCutsceneFrameDelay);
			if (_skip) {
				return;
			}
			if (!_g->_cut.play()) {
				_g->_cut.unload();
				if (!_g->_cut.isInterrupted()) {
					do {
						_g->_cut._numToPlay = getNextIntroCutsceneNum(_g->_cut._numToPlay);
					} while (_g->_cut._numToPlay >= 0 && !_g->_cut.load(_g->_cut._numToPlay));
				} else {
					_g->_cut._numToPlay = -1;
				}
				if (_g->_cut._numToPlay < 0) {
					_nextState = kStateGame;
				}
			}
			break;
		case kStateGame:
			if (_g->_changeLevel) {
				_g->_changeLevel = false;
				warning("_changeLevel flag set, starting level %d", _g->_level);
				_g->initLevel();
			} else if (_g->_endGame) {
				_g->_endGame = false;
				warning("_endGame flag set, starting level %d", _g->_level);
				_g->initLevel();
			}
			_g->updateGameInput();
			_g->doTick();
			if (_g->inp.inventoryKey) {
				_g->inp.inventoryKey = false;
				_nextState = kStateInventory;
//			} else if (_g->inp.escapeKey) {
//				_g->inp.escapeKey = false;
//				_nextState = kStateMenu;
			} else if (_g->_cut._numToPlay >= 0) {
				_nextState = kStateCutscene;
			} else if (_g->_boxItemCount != 0) {
				_nextState = kStateBox;
			}
			break;
		case kStateInventory:
			_g->updateInventoryInput();
			_g->doInventory();
			if (_g->inp.inventoryKey || _g->inp.escapeKey) {
				_g->inp.inventoryKey = false;
				_g->inp.escapeKey = false;
				_g->closeInventory();
				_nextState = kStateGame;
			}
			break;
		case kStateBox:
			_g->doBox();
			if (_g->_boxItemCount == 0) {
				_nextState = kStateGame;
			}
			break;
		case kStateMenu:
			_g->doMenu();
			if (_g->inp.escapeKey) {
				_g->inp.escapeKey = false;
				_nextState = kStateGame;
			}
			break;
		}
	}
	virtual void initGL(int w, int h) {
		_render->resizeScreen(w, h);
	}
	virtual void drawGL() {
		_render->drawOverlay();
		if (_loadState) {
			if (_state == kStateGame) {
				_g->loadGameState(_slotState);
				debug(kDebug_INFO, "Loaded game state from slot %d", _slotState);
			}
			_loadState = false;
		}
		if (_saveState) {
			if (_state == kStateGame) {
				_g->saveGameState(_slotState);
				_g->saveScreenshot(_slotState);
				debug(kDebug_INFO, "Saved game state to slot %d", _slotState);
			}
			_saveState = false;
		}
	}
	virtual void saveState(int slot) {
		_slotState = slot;
		_saveState = true;
	}
	virtual void loadState(int slot) {
		_slotState = slot;
		_loadState = true;
	}
};

extern "C" {
	GameStub *GameStub_create() {
		return new GameStub_F2B;
	}
};
