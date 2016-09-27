/*
 * Fade To Black engine rewrite
 * Copyright (C) 2006-2012 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "decoder.h"
#include "file.h"
#include "game.h"
#include "trigo.h"
#include "resource.h"
#include "render.h"

Game::Game(Render *render, const GameParams *params)
	: _cut(render, this, &_snd), _snd(&_res), _render(render), _params(*params) {

	_cheats = 0;

	memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));

	_ticks = 0;
	_level = 0;
	_skillLevel = kSkillNormal;
	_changeLevel = false;
	_room = _roomPrev = -1;

	_zTransform = 8;
	_viewportSize = 0;

	_currentObject = 0;
	memset(_objectKeysTable, 0, sizeof(_objectKeysTable));

	_changedObjectsCount = 0;
	memset(&_infoPanelSpr, 0, sizeof(_infoPanelSpr));
	memset(_fontsTable, 0, sizeof(_fontsTable));
	memset(_sceneAnimationsTextureTable, 0, sizeof(_sceneAnimationsTextureTable));
	memset(_sceneTextureImagesBuffer, 0, sizeof(_sceneTextureImagesBuffer));
	memset(_sceneObjectsTable, 0, sizeof(_sceneObjectsTable));
	memset(_sceneCellMap, 0, sizeof(_sceneCellMap));
	memset(_playerMessagesTable, 0, sizeof(_playerMessagesTable));
}

Game::~Game() {
}

void Game::clearGlobalData() {
	memset(_varsTable, 0, sizeof(_varsTable));
	memset(_gunTicksTable, 0, sizeof(_gunTicksTable));
	memset(_objectsPtrTable, 0, sizeof(_objectsPtrTable));
	_currentObject = 0;
	memset(_objectKeysTable, 0, sizeof(_objectKeysTable));
	memset(&_tmpObject, 0, sizeof(_tmpObject));
	_changedObjectsCount = 0;
	memset(_changedObjectsTable, 0, sizeof(_changedObjectsTable));
	_followingObjectsCount = 0;
	_followingObjectsTable = 0;
	_currentObjectKey = 0;
	_conradObjectKey = _worldObjectKey = 0;
	_currentScriptKey = 0;
	_newPlayerObject = 0;
	_objectsCount = _objectsSetupCount = 0;
	_objectsDrawCount = 0;
	memset(_objectsDrawList, 0, sizeof(_objectsDrawList));
	_updateGlobalPosRefObject = 0;
	_collidingObjectsCount = 0;
	memset(_collidingObjectsTable, 0, sizeof(_collidingObjectsTable));
	_object10Counter = 0;
	_conradEnvAni = 0;
	_boxItemCount = 0;
	_boxItemObj = 0;

	memset(&inp, 0, sizeof(inp));
	_inputsCount = 0;
	_inputsTable = 0;
	memset(_inputDirKeyReleased, 0, sizeof(_inputDirKeyReleased));
	memset(_inputDirKeyPressed, 0, sizeof(_inputDirKeyPressed));
	memset(_inputButtonKey, 0, sizeof(_inputButtonKey));
	_inputKeyAction = _inputKeyUse = _inputKeyJump = false;
}

void Game::clearLevelData() {
	_cut._numToPlayCounter = -1;
	_cut._numToPlay = -1;

	_fixedViewpoint = false;
	_xPosObserverPrev = _xPosObserver = 0;
	_yPosObserverPrev = _yPosObserver = 0;
	_zPosObserverPrev = _zPosObserver = 0;
	_yRotObserver = -24;
	_yInvRotObserver = -_yRotObserver;
	_yRotObserverPrev2 = _yRotObserverPrev = _yRotObserver - 1;
	_yPosObserverTemp = _yRotObserver;
	if (_level != 6 && _level != 12) {
		_cameraDefaultDist = 1;
		_cameraStandingDist = 1;
		_cameraUsingGunDist = 0;
		_conradUsingGun = false;
	} else {
		_cameraDefaultDist = 0;
		_cameraStandingDist = 0;
		_cameraUsingGunDist = 0;
		_conradUsingGun = false;
	}
	_cameraDistInitDone = true;
	_yCosObserver = _ySinObserver = _yInvCosObserver = _yInvSinObserver = 0;
	_yPosObserverValue = _yPosObserverValue2 = 0;
	_yPosObserverTicks = 2;
	_xPosViewpoint = _yPosViewpoint = _zPosViewpoint = 0;
	_x0PosViewpoint = _y0PosViewpoint = _z0PosViewpoint = 0;
	_yRotViewpoint = 0;

	_cameraDeltaRotY = 0;
	_cameraDeltaRotYValue2 = 0;
	_cameraDeltaRotYTicks = 3;
	_cameraViewObj = 0;
	_cameraDistValue = _cameraDist = (kWallWidth * 2) << 15;
	_cameraDistTicks = 2;
	_cameraDistValue2 = 0;
	_cameraStep1 = _cameraStep2 = 8;
	_pitchObserverCamera = _observerColMask = 0;
	_prevCameraState = _cameraState = 0;
	_currentCamera = 0;
	_cameraDx = _cameraDy = _cameraDz = 0;

	_rnd.reset();

	_spriteCache.flush();
	_infoPanelSpr.data = 0;
	_render->flushCachedTextures();

	for (int i = 0; i < ARRAYSIZE(_objectKeysTable); ++i) {
		GameObject *o = _objectKeysTable[i];
		if (o) {
			GameMessage *msg = o->msg;
			while (msg) {
				GameMessage *next = msg->next;
				free(msg);
				msg = next;
			}
			o->msg = 0;
		}
	}
}

void Game::countObjects(int16_t parentKey) {
	int16_t key;

	uint8_t *p = _res.getData(kResType_OBJ, parentKey, "OBJ");
	const char *name = (const char *)p + 232;
	_res.setObjectKey(name, parentKey);

	++_objectsCount;
	key = _res.getChild(kResType_OBJ, parentKey);
	if (key) {
		countObjects(key);
	}
	key = _res.getNext(kResType_OBJ, parentKey);
	if (key) {
		countObjects(key);
	}
}

int Game::gotoStartScriptAnim() {
	GameObject *o = _currentObject;
	GameObjectAnimation *anim = &o->anim;
	anim->animKey = READ_LE_UINT16(o->scriptCondData + 2);
	anim->aniheadData = _res.getData(kResType_ANI, anim->animKey, "ANIHEAD");
	if (READ_LE_UINT16(anim->aniheadData + 6) != 0) {
		if (READ_LE_UINT16(anim->aniheadData + 8) == 0) {
			int32_t args[] = { 0, 0 };
			op_playSound(2, args);
		}
		if (READ_LE_UINT16(anim->aniheadData + 8) == 0xFFFF) {
			int32_t args[] = { 0, 2 };
			op_playSound(2, args);
		}
	}
	anim->currentAnimKey = _res.getChild(kResType_ANI, anim->animKey);
	anim->anikeyfData = _res.getData(kResType_ANI, anim->currentAnimKey, "ANIKEYF");
	assert(anim->anikeyfData != 0);
	if (o->flags[1] & 0x4) {
		for (int i = 0; i < 4; ++i) {
			int index = o->specialData[1][i];
			if (index < 0) {
				break;
			}
			const int frameNum = o->specialData[1][4 + i];
			SceneAnimation *sa = &_sceneAnimationsTable[index];
			sa->type &= ~2;
			if (!_res.getNext(kResType_ANI, o->anim.currentAnimKey)) {
				sa->type |= 0xC;
			} else {
				sa->type |= 0xE;
			}
			sa->frameNum = frameNum;
			sa->frm2Key = o->anim.animKey;
			sa->msg = 71;
			sa->msgKey = o->objKey;
		}
	} else if (o->flags[1] & 0x80) {
	} else if (o->flags[1] & 0x8) {
		int16_t cond_dx = (int8_t)o->scriptCondData[7];
		int16_t cond_dy = (int8_t)o->scriptCondData[8];
		int16_t cond_dz = (int8_t)o->scriptCondData[9];
		int16_t anikeyf_x0 = READ_LE_UINT16(anim->anikeyfData + 2);
		int16_t anikeyf_z0 = READ_LE_UINT16(anim->anikeyfData + 6);
		if (!updateGlobalPos(-cond_dx * 16 + anikeyf_x0, cond_dy * 32, cond_dz * 16 - anikeyf_z0, -cond_dx, cond_dz, 0)) {
			return 1;
		}
	}
	anim->framesCount = 0;
	anim->ticksCount = 0;
	return 0;
}

int Game::gotoNextScriptAnim() {
	int16_t nextKey;
	GameObject *o = _currentObject;
	GameObjectAnimation *anim = &o->anim;
	if (anim->anikeyfData == 0) {
		anim->anikeyfData = _res.getData(kResType_ANI, anim->currentAnimKey, "ANIKEYF");
	}
	uint8_t *p_anikeyf = anim->anikeyfData;
	if (anim->ticksCount >= p_anikeyf[0] - 1) {
		nextKey = _res.getNext(kResType_ANI, anim->currentAnimKey);
		if (nextKey == 0) {
			anim->anikeyfData = 0;
			return 0;
		}
		anim->currentAnimKey = nextKey;
		anim->anikeyfData = _res.getData(kResType_ANI, anim->currentAnimKey, "ANIKEYF");
		p_anikeyf = anim->anikeyfData;
		++anim->framesCount;
		anim->ticksCount = 0;
	} else {
		++anim->ticksCount;
		nextKey = anim->currentAnimKey;
	}
	if (READ_LE_UINT16(anim->aniheadData + 6) != 0) {
		if (READ_LE_UINT16(anim->aniheadData + 8) == anim->framesCount) {
			int32_t args[] = { 0, 0 };
			op_playSound(2, args);
		}
		if (READ_LE_UINT16(anim->aniheadData + 8) == 0xFFFF) {
			int32_t args[] = { 0, 2 };
			op_playSound(2, args);
		}
	}
	if (nextKey != 0 && (o->flags[1] & 0x8) != 0) {
		int16_t anikeyf_x0 = READ_LE_UINT16(p_anikeyf + 2);
//		int16_t anikeyf_y0 = READ_LE_UINT16(p_anikeyf + 4);
		int16_t anikeyf_z0 = READ_LE_UINT16(p_anikeyf + 6);
		int16_t anikeyf_dx = READ_LE_UINT16(p_anikeyf + 8);
		int16_t anikeyf_dy = READ_LE_UINT16(p_anikeyf + 10);
		int16_t anikeyf_dz = READ_LE_UINT16(p_anikeyf + 12);
		if (!updateGlobalPos(-anikeyf_dx + anikeyf_x0, anikeyf_dy, anikeyf_dz - anikeyf_z0, -anikeyf_dx, anikeyf_dz, 1)) {
			return 1;
		}
	}
	return 0;
}

void Game::gotoEndScriptAnim(int fl) {
	GameObject *o = _currentObject;
	while (o->anim.anikeyfData) {
		int16_t nextKey = _res.getNext(kResType_ANI, o->anim.currentAnimKey);
		if (nextKey == 0) {
			o->anim.anikeyfData = 0;
			break;
		}
		o->anim.currentAnimKey = nextKey;
	}
	if (fl == 1) {
		o->xPos = o->xPosPrev;
		o->yPos = o->yPosPrev;
		o->zPos = o->zPosPrev;
	}
}

int16_t Game::getObjectScriptAnimKey(int16_t groupKey, int num) {
	int16_t animKey = _res.getChild(kResType_ANI, groupKey);
	while (animKey != 0) {
		const uint8_t *p = _res.getData(kResType_ANI, animKey, "ANIHEAD");
		if (p && num == READ_LE_UINT16(p + 14)) {
			return animKey;
		}
		animKey = _res.getNext(kResType_ANI, animKey);
	}
	return 0;
}

void Game::setObjectFlags(const uint8_t *p, GameObject *o) {
	for (int i = 0; i <= 18; ++i) {
		const uint32_t bitmask = 1 << i;
		if (p[40 + i]) {
			o->flags[0] |= bitmask;
			o->flags[1] |= bitmask;
		} else {
			o->flags[0] &= ~bitmask;
			o->flags[1] &= ~bitmask;
		}
	}
}

int Game::getObjectFlag(GameObject *o, int flag) {
	assert(flag >= 96 && flag <= 114);
	const uint32_t bitmask = 1 << (flag - 96);
	return (o->flags[1] & bitmask) != 0;
}

void Game::setObjectFlag(GameObject *o, int flag, int value) {
	assert(flag >= 96 && flag <= 114);
	const uint32_t bitmask = 1 << (flag - 96);
	if (value) {
		o->flags[1] |= bitmask;
	} else {
		o->flags[1] &= ~bitmask;
	}
}

void Game::setupObjectScriptAnim(GameObject *o) {
	GameObjectAnimation *anim = &o->anim;

	anim->aniheadData = 0;
	anim->anikeyfData = 0;
	anim->aniframData = 0;

	if (anim->animKey > 0) {
		anim->aniheadData = _res.getData(kResType_ANI, anim->animKey, "ANIHEAD");
	}
	if (anim->currentAnimKey > 0) {
		anim->anikeyfData = _res.getData(kResType_ANI, anim->currentAnimKey, "ANIKEYF");
		if ((o->flags[1] & 0x80) == 0) {
			int16_t frameKey = _res.getChild(kResType_ANI, anim->currentAnimKey);
			if (frameKey > 0) {
				anim->aniframData = _res.getData(kResType_ANI, frameKey, "ANIFRAM");
			}
		}
	}
}

void Game::setupObjectsChangeOrder(GameObject *o) {
	GameObject *o_prev, *o_cur, *o_tmp;

	o_prev = o_cur = o->o_child;
	if (o_cur->o_next) {
		o_cur = o_cur->o_next;
		do {
			o_tmp = o_cur->o_next;
			if ((o_cur->flags[1] & 0x100) == 0) {
				o_prev->o_next = o_cur->o_next;
				o_cur->o_next = o_cur->o_parent->o_child;
				o_cur->o_parent->o_child = o_cur;
			} else {
				o_prev = o_cur;
			}
			o_cur = o_tmp;
		} while (o_cur);
	}
}

static const struct {
	const char *name;
	int index;
} _gameObjectsPtrinitTable[] = {
	{ "conrad", kObjPtrConrad },
	{ "cimetiere", kObjPtrCimetiere },
	{ "objet_temporaire", kObjPtrObjetTemporaire },
	{ "inventaire", kObjPtrInventaire },
	{ "fond_scan_info", kObjPtrFondScanInfo },
	{ "icones_souris", kObjPtrIconesSouris },
	{ "cible", kObjPtrCible },
	{ "target_command", kObjPtrTargetCommand },
	{ "target", kObjPtrTarget },
	{ "save_load_option", kObjPtrSaveloadOption },
	{ "scenes_cine", kObjPtrScenesCine },
	{ "fade_to_black", kObjPtrFadeToBlack }
};

GameObject *Game::setupObjectsHelper(int16_t prevKey, GameObject *o_parent) {
	int16_t childKey, key;
	GameObject *o_prev, *o_new = 0;
	bool hasNext = false;

	if (prevKey == 0) {
		key = _res.getRoot(kResType_OBJ);
	} else {
		key = prevKey;
	}
	assert(key != 0);
	do {
		++_objectsSetupCount;
		uint8_t *p = _res.getData(kResType_OBJ, key, "OBJ");
		o_prev = o_new;
		if (prevKey == 0) {
			o_new = o_parent;
		} else {
			o_new = _currentObject + 1;
		}
		_currentObject = o_new;
		if (hasNext) {
			o_prev->o_next = o_new;
		} else {
			o_parent->o_child = o_new;
		}
		if (!o_parent) {
			o_parent = o_new;
		}
		o_new->o_parent = o_parent;
		o_new->objKey = key;
		o_new->scriptKey = READ_LE_UINT16(p);
		o_new->name = (const char *)p + 232;
		if (strcmp(o_new->name, "conrad") == 0) {
			_conradObjectKey = key;
		} else if (strcmp(o_new->name, "world") == 0) {
			_worldObjectKey = key;
		}
		if (p[48]) {
			o_new->anim.animKey = READ_LE_UINT16(p + 2);
		} else {
			o_new->startScriptKey = READ_LE_UINT16(p + 2);
			if (o_new->scriptKey != 0) {
				uint8_t *q = _res.getData(kResType_STM, o_new->scriptKey, "STMHEADE");
				o_new->anim.animKey = getObjectScriptAnimKey(READ_LE_UINT16(q), READ_LE_UINT16(p + 4));
			}
		}
		o_new->anim.currentAnimKey = _res.getChild(kResType_ANI, o_new->anim.animKey);
		setObjectFlags(p, o_new);
		setupObjectScriptAnim(o_new);
		o_new->anim.ticksCount = 1;
		o_new->pal = READ_LE_UINT32(p + 180);
		assert(p[6] == 255 || p[6] == 0);
		o_new->state = (p[6] != 0) ? 1 : 0;
		o_new->inSceneList = 1;
		o_new->updateColliding = true;
		o_new->xPosPrev = o_new->xPos = READ_LE_UINT32(p + 8);
		o_new->yPosPrev = o_new->yPos = READ_LE_UINT32(p + 12);
		o_new->zPosPrev = o_new->zPos = READ_LE_UINT32(p + 16);
		o_new->pitchPrev = o_new->pitch = READ_LE_UINT32(p + 20);
		if (strcmp(o_new->name, "conrad") == 0) {
			if (_params.xPosConrad > 0) {
				o_new->xPosPrev = o_new->xPos = _params.xPosConrad << 19;
			}
			if (_params.zPosConrad > 0) {
				o_new->zPosPrev = o_new->zPos = _params.zPosConrad << 19;
			}
		}
		o_new->xFrm1 = READ_LE_UINT32(p + 24);
		o_new->zFrm1 = READ_LE_UINT32(p + 28);
		o_new->xFrm2 = READ_LE_UINT32(p + 32);
		o_new->zFrm2 = READ_LE_UINT32(p + 36);
		o_new->room = READ_LE_UINT32(p + 176);
		for (int i = 0; i < 12; ++i) {
			o_new->customData[i] = READ_LE_UINT32(p + 184 + i * 4);
		}
		if (p[295] == 1) {
			o_new->customData[2] = READ_LE_UINT32(p + 8);
			o_new->customData[3] = READ_LE_UINT32(p + 16);
			o_new->xPosPrev = o_new->xPos = 0;
			o_new->yPosPrev = o_new->yPos = 0;
			o_new->zPosPrev = o_new->zPos = 0;
			_roomsTable[o_new->room].palKey = o_new->pal;
			_roomsTable[o_new->room].o = o_new;
			if (_roomsTableCount < o_new->room) {
				_roomsTableCount = o_new->room;
			}
		}
		if ((o_new->flags[1] & 0x8000) != 0) {
			o_new->customData[11] = _followingObjectsCount;
			o_new->customData[10] = -1;
			++_followingObjectsCount;
		}
		if ((o_new->flags[1] & 0x10000) != 0) {
			o_new->customData[11] = _inputsCount;
			++_inputsCount;
		}
		for (int i = 0; i < 26; ++i) {
			o_new->specialData[0][i] = o_new->specialData[1][i] = READ_LE_UINT32(p + 72 + i * 4);
		}
		o_new->colSlot = 0;
		if (o_new->xPos >= 0 || o_new->zPos >= 0) {
			_currentObject->xPosParent = o_parent->xPosParent + o_parent->xPos;
			_currentObject->yPosParent = o_parent->yPosParent + o_parent->yPos;
			_currentObject->zPosParent = o_parent->zPosParent + o_parent->zPos;
			const uint32_t fl = _currentObject->flags[1];
			_currentObject->flags[1] &= ~0x100;
			initCollisionSlot(_currentObject);
			_currentObject->flags[1] = fl;
		}
		o_new->updateColliding = false;
		if (getMessage(o_new->objKey, 32767, &_tmpMsg)) {
			o_new->text = (const char *)_tmpMsg.data;
		} else {
			o_new->text = o_new->name;
		}
		if ((o_new->flags[1] & 0x100) == 0) {
			o_new->scriptStateKey = o_new->startScriptKey;
			if (o_new->scriptStateKey != 0) {
				o_new->scriptStateData = _res.getData(kResType_STM, o_new->scriptStateKey, "STMSTATE");
			}
		}
		o_new->setColliding = false;
		o_new->msg = 0;
		debug(kDebug_GAME, "Game::setupObjectsHelper() object_name '%s'", o_new->name);
		for (int i = 0; i < ARRAYSIZE(_gameObjectsPtrinitTable); ++i) {
			if (strcmp(_gameObjectsPtrinitTable[i].name, o_new->name) == 0) {
				_objectsPtrTable[_gameObjectsPtrinitTable[i].index] = o_new;
				break;
			}
		}
		if (strcmp(o_new->name, "conrad") == 0) {
			_xPosViewpoint = _x0PosViewpoint = o_new->xPos;
			_yPosViewpoint = _y0PosViewpoint = o_new->yPos;
			_zPosViewpoint = _z0PosViewpoint = o_new->zPos;
			_yRotObserver = o_new->pitch;
			int cosy =  g_cos[_yRotObserver & 1023];
			int siny = -g_sin[_yRotObserver & 1023];
			_xPosObserver = _xPosViewpoint - fixedMul(cosy, _cameraDist, 15);
			_zPosObserver = _zPosViewpoint - fixedMul(siny, _cameraDist, 15);
			debug(kDebug_GAME, "Game::setupObjectsHelper() conrad initial pos %d,%d,%d", o_new->xPos, o_new->yPos, o_new->zPos);
			_conradEnvAni = o_new->specialData[1][20];
			_varsTable[kVarPlayerObject] = _objectsPtrTable[kObjPtrConrad]->objKey;
		}
		o_new->xPosParentPrev = o_new->xPosParent = o_parent->xPosParent + o_parent->xPos;
		o_new->yPosParentPrev = o_new->yPosParent = o_parent->yPosParent + o_parent->yPos;
		o_new->zPosParentPrev = o_new->zPosParent = o_parent->zPosParent + o_parent->zPos;
		assert(o_new->objKey >= 0 && o_new->objKey < kObjectKeysTableSize && _objectKeysTable[o_new->objKey] == 0);
		_objectKeysTable[o_new->objKey] = o_new;
		childKey = _res.getChild(kResType_OBJ, key);
		if (childKey != 0) {
			setupObjectsHelper(childKey, o_new);
			setupObjectsChangeOrder(o_new);
		} else {
			o_new->o_child = 0;
		}

		key = _res.getNext(kResType_OBJ, key);
		hasNext = key != 0;
	} while (hasNext);
	return o_new;
}

void Game::setupObjects() {
	debug(kDebug_GAME, "Game::setupObjects()");

	_roomsTableCount = -1;
	memset(_roomsTable, 0, sizeof(_roomsTable));

	_objectsCount = 0;
	countObjects(_res.getRoot(kResType_OBJ));
	debug(kDebug_GAME, "Game::setupObjects() _objectsCount=%d", _objectsCount);

	GameObject *o_world = ALLOC<GameObject>(_objectsCount);

	_objectsSetupCount = 0;
	_inputsCount = 0;
	_followingObjectsCount = 0;

	_objectsPtrTable[kObjPtrWorld] = o_world;
	setupObjectsHelper(0, o_world);
	debug(kDebug_GAME, "Game::setupObjects() _objectsCount %d _objectsSetupCount %d", _objectsCount, _objectsSetupCount);
	assert(_objectsSetupCount == _objectsCount);

	free(_followingObjectsTable);
	_followingObjectsTable = 0;
	if (_followingObjectsCount != 0) {
		_followingObjectsTable = ALLOC<GameFollowingObject>(_followingObjectsCount);
	}

	free(_inputsTable);
	_inputsTable = 0;
	if (_inputsCount != 0) {
		_inputsTable = (GameInput *)calloc(_inputsCount, sizeof(GameInput));
		if (_inputsTable) {
			for (int i = 0; i < _inputsCount; ++i) {
				GameInput *inp = &_inputsTable[i];
				memset(inp, 0, sizeof(GameInput));
				inp->inputKey0 = 0;
				inp->inputKey1 = 1;
			}
		}
	}

	for (int i = 0; i < kRoomsTableSize; ++i) {
		GameRoom *room = &_roomsTable[i];
		if (room->palKey != 0) {
			debug(kDebug_GAME, "Game::setupObjects() room %d '%s' keyPal %d", i, room->o->name, room->palKey);
			room->palKey = _palKeysTable[room->palKey];
		}
	}
}

void Game::getAllPalKeys(int16_t mapKey) {
	memset(_palKeysTable, 0, sizeof(_palKeysTable));
	const uint8_t *p = _res.getData(kResType_MAP, mapKey, "MAP3D");
	if (p) {
		int count = READ_LE_UINT32(p + 28);
		assert(count >= 0 && count < kPalKeysTableSize);
		for (int i = 0; i < count; ++i) {
			int16_t palKey = READ_LE_UINT16(p + 32 + i * 2);
			debug(kDebug_GAME, "Game::getAllPalKeys() mapKey %d pal %d key %d", mapKey, i, palKey);
			if (palKey != 0) {
				_palKeysTable[i] = palKey;
			}
		}
	}
}

void Game::getSceneAnimationTexture(SceneAnimation *sa, uint16_t *len, uint16_t *flags, SpriteImage *spr) {
	// seek to the correct frame
	int16_t key = _res.getChild(kResType_ANI, sa->frmKey);
	assert(key != 0);
	for (int i = 0; i < sa->frameNum; ++i) {
		key = _res.getNext(kResType_ANI, key);
		if (key == 0) {
			warning("getSceneAnimationTexture() key is 0 for frame %d/%d frmKey %d", i, sa->frameNum, sa->frmKey);
			return;
		}
	}
	const uint8_t *p_frm = _res.getData(kResType_ANI, key, "ANIFRAM");
	const uint8_t *p_btm = _res.getData(kResType_SPR, READ_LE_UINT16(p_frm), "BTMDESC");
	spr->w = READ_LE_UINT16(p_btm);
	spr->h = READ_LE_UINT16(p_btm + 2);
	spr->data = _res.getData(kResType_SPR, READ_LE_UINT16(p_frm), "SPRDATA");
	spr->key = READ_LE_UINT16(p_frm);
	const uint8_t *p_key = _res.getData(kResType_ANI, sa->frmKey, "ANIKEYF");
	if (len) {
		*len = p_key[0];
	}
	if (flags) {
		*flags = p_key[1];
	}
}

void Game::loadColorMark(int16_t key) {
	if (key == 0) {
		warning("Game::loadColorMark() invalid palette key");
		return;
	}
	int16_t colorKey = _res.getChild(kResType_PAL, key);
	const uint8_t *p = _res.getData(kResType_PAL, colorKey, "MRKCOLOR");
	for (int i = 0; i < 260; ++i) {
		_mrkBuffer[i] = READ_LE_UINT32(p);
		p += 4;
	}
}

static int quantizeColor(int r, int g, int b) {
	const int q = r * 19661 + g * 38666 + b * 7209;
	return q;
}

void Game::setupIndirectColorPalette() {
	for (int i = 0; i < 256; ++i) {
		if (i == 0) {
			_indirectPalette[kIndirectColorShadow][i] = 0;
		} else if (i == 255 || i == 254) {
			_indirectPalette[kIndirectColorShadow][i] = 255;
		} else if ((_mrkBuffer[i + 1] & 1) != 0 || (_mrkBuffer[i + 2] & 1) != 0) {
			_indirectPalette[kIndirectColorShadow][i] = i;
		} else {
			_indirectPalette[kIndirectColorShadow][i] = i + 1;
		}
		if (i == 0) {
			_indirectPalette[kIndirectColorLight][i] = 0;
		} else if ((_mrkBuffer[i] & 1) != 0 || (_mrkBuffer[i - 1] & 1) != 0) {
			_indirectPalette[kIndirectColorShadow][i] = i;
		} else {
			_indirectPalette[kIndirectColorShadow][i] = i - 1;
		}
	}
	int q[256];
	for (int ind = 0; ind < 4; ++ind) {
		int startColor = _mrkBuffer[256 + ind];
		int color = 0;
		do {
			int r = _screenPalette[(startColor + color) * 3];
			int g = _screenPalette[(startColor + color) * 3 + 1];
			int b = _screenPalette[(startColor + color) * 3 + 2];
			q[color] = quantizeColor(r, g, b);
			++color;
		} while (startColor + color < 256 && _mrkBuffer[startColor + color] == 0);
		for (int i = 0; i < 256 * 3; i += 3) {
			int col_q = quantizeColor(_screenPalette[i], _screenPalette[i + 1], _screenPalette[i + 2]);
			int min_q = 0;
			for (int j = 0; j < color; ++j) {
				int tmp_q = ABS(col_q - q[j]);
				if (j == 0 || tmp_q < min_q) {
					min_q = tmp_q;
					_indirectPalette[kIndirectColorGreen + ind][i / 3] = startColor + j;
				}
			}
		}
	}
}

void Game::setPalette(int16_t key) {
	if (key == 0) {
		warning("Game::setPalette() invalid palette key");
		return;
	}
	const uint8_t *p = _res.getData(kResType_PAL, key, "PALDATA");
	memcpy(_screenPalette, p, 256 * 3);
	_render->setPalette(p, 256);
}

void Game::updatePalette() {
	GameObject *o = _objectsPtrTable[kObjPtrConrad];
	o->room = getCellMapShr19(o->xPos, o->zPos)->room;
	int16_t palKey = _roomsTable[o->room].palKey;
	debug(kDebug_GAME, "Game::updatePalette() room %d palKey %d", o->room, palKey);
	loadColorMark(palKey);
	setPalette(palKey);
	setupIndirectColorPalette();
}

void Game::loadSceneMap(int16_t key) {
#ifdef BUFFER_TEXTPOLYGONS
	_render->flushQuads();
#endif
	assert(key != 0);
	uint8_t *p = _res.getData(kResType_MAP, key, "MAP3D");
	_sceneCamerasCount = READ_LE_UINT32(p + 24);
	_sceneAnimationsCount = READ_LE_UINT32(p + 12);
	assert(_sceneAnimationsCount < 512);
	_sceneAnimationsCount2 = READ_LE_UINT32(p + 16);
	assert(_sceneAnimationsCount2 <= _sceneAnimationsCount - 2);
	int palettesCount = READ_LE_UINT32(p + 28);
	debug(kDebug_GAME, "Game::loadSceneMap() key %d cameras %d palettes %d animations %d", key, _sceneCamerasCount, palettesCount, _sceneAnimationsCount);
//	memset(_sceneGridX, 0, sizeof(_sceneGridX));
//	memset(_sceneGridZ, 0, sizeof(_sceneGridZ));
	uint32_t dataOffset = READ_LE_UINT32(p);
	uint8_t *q = _res.getData(kResType_MAP, key, "MAPDATA");
	assert(q == p + dataOffset);
	for (int x = 0; x < 64; ++x) {
		for (int z = 0; z < 64; ++z) {
			CellMap *cell = &_sceneCellMap[x][z];
			cell->texture[0] = READ_LE_UINT16(q + 2);
			cell->texture[1] = READ_LE_UINT16(q + 4);
			cell->data[0] = q[14];
			cell->data[1] = q[15];
			cell->camera = q[16];
			cell->camera2 = q[17];
			cell->room2 = q[18];
			cell->room = q[19];
			cell->type = READ_LE_UINT16(q);
			debug(kDebug_GAME, "cell[%d][%d] camera %d/%d room %d/%d type %d texture %d/%d data %d/%d", x, z, cell->camera, cell->camera2, cell->room, cell->room2, cell->type, cell->texture[0], cell->texture[1], cell->data[0], cell->data[1]);
			if (cell->type == 255) {
				cell->type = -1;
			}
			if (cell->type == -2 || cell->type == -3) {
				cell->type = 0;
			}
			if (cell->type > 0) {
//				_sceneGridZ[x][z] = (int16_t)READ_LE_UINT16(q + 6); // w
//				_sceneGridX[x][z] = (int16_t)READ_LE_UINT16(q + 12); // s
//				assert(z + 1 < 64);
//				_sceneGridX[x][z + 1] = (int16_t)READ_LE_UINT16(q + 8); // n
//				assert(x + 1 < 64);
//				_sceneGridZ[x + 1][z] = (int16_t)READ_LE_UINT16(q + 10); // e
				cell->west = READ_LE_UINT16(q + 6);
				cell->north = READ_LE_UINT16(q + 8);
				cell->east = READ_LE_UINT16(q + 10);
				cell->south = READ_LE_UINT16(q + 12);
			}
			q += 20;
		}
	}
	dataOffset = READ_LE_UINT32(p + 4);
	q = _res.getData(kResType_MAP, key, "GDATA");
	assert(q == p + dataOffset);
	for (int x = 0; x < 64; ++x) {
		for (int z = 0; z < 64; ++z) {
			_sceneGroundMap[x][z] = READ_LE_UINT32(q);
			q += 4;
		}
	}
	dataOffset = READ_LE_UINT32(p + 20);
	q = _res.getData(kResType_MAP, key, "CAMDATA");
	assert(q == p + dataOffset);
	for (int i = 0; i < _sceneCamerasCount; ++i) {
		CameraPosMap *camPos = &_sceneCameraPosTable[i];
		camPos->x = READ_LE_UINT32(q); q += 4;
		camPos->z = READ_LE_UINT32(q); q += 4;
		camPos->ry = READ_LE_UINT32(q); q += 4;
		camPos->l_ry = READ_LE_UINT32(q); q += 4;
		camPos->r_ry = READ_LE_UINT32(q); q += 4;
	}
	dataOffset = READ_LE_UINT32(p + 8);
	q = _res.getData(kResType_MAP, key, "ANIDATA");
	assert(q == p + dataOffset);
	for (int i = 0; i < _sceneAnimationsCount; ++i) {
		SceneAnimation *sa = &_sceneAnimationsTable[i];
		sa->aniKey = READ_LE_UINT16(q + 8);
		sa->frmKey = READ_LE_UINT16(q + 10);
		sa->ticksCount = READ_LE_UINT32(q + 12);
		sa->framesCount = READ_LE_UINT32(q + 16);
		sa->typeInit = sa->type = READ_LE_UINT32(q + 20);
		sa->frameNumInit = sa->frameNum = READ_LE_UINT32(q + 24);
		sa->frm2Key = READ_LE_UINT16(q + 28);
		sa->msgKey = READ_LE_UINT16(q + 30);
		sa->msg = READ_LE_UINT32(q + 32);
		sa->next = READ_LE_UINT32(q + 36);
		sa->prevFrame = 0;
		sa->direction = 1;
		sa->pauseTicksCount = 1;
		sa->frameIndex = 0;
		sa->frame2Index = READ_LE_UINT16(q + 48);
		sa->flags = 0;
		if (sa->next < 2 || sa->next >= _sceneAnimationsCount) {
			sa->next = i;
		}
		if (sa->aniKey != 0) {
#if 0
			p = _res.getData(kResType_ANI, sa->aniKey, 0);
			if (p == 0) {
				if ((sa->type & 1) == 1) {
					_sceneAnimationsTextureTable[i].data = _sceneDefaultGroundTexture;
				} else {
					_sceneAnimationsTextureTable[i].data = _sceneDefaultWallTexture;
				}
				sa->type &= ~2;
				sa->aniKey = 0;
			} else
#endif
			sa->framesCount = 0;
			int16_t aniKey = _res.getChild(kResType_ANI, sa->aniKey);
			while (aniKey != 0) {
				p = _res.getData(kResType_ANI, aniKey, "ANIKEYF");
				assert(p);
				if ((p[0] & 0x1C) == 0x18) {
					sa->frame2Index = sa->framesCount - 1;
				}
				++sa->framesCount;
				aniKey = _res.getNext(kResType_ANI, aniKey);
			}
			assert(sa->framesCount > 0);
			SceneAnimationState *sas = &_sceneAnimationsStateTable[i];
			getSceneAnimationTexture(sa, &sas->len, &sas->flags, &_sceneAnimationsTextureTable[i]);
			sa->pauseTicksCount = sas->len;
		} else {
			debug(kDebug_GAME, "Game::loadSceneMap() i %d type 0x%X", i, sa->type);
			switch (i) {
			case 0:
				_sceneAnimationsTextureTable[i].w = 16;
				_sceneAnimationsTextureTable[i].h = 16;
				_sceneAnimationsTextureTable[i].data = 0;
				break;
			case 1:
				_sceneAnimationsTextureTable[i].w = 64;
				_sceneAnimationsTextureTable[i].h = 16;
				_sceneAnimationsTextureTable[i].data = 0;
				break;
			default:
				assert(0);
			}
		}
		q += 52;
	}
}

bool Game::updateSceneAnimationsKeyFrame(int rnd, int index, SceneAnimation *sa, SceneAnimationState *sas) {
	int16_t prevFrameIndex = sa->frameIndex;
	switch (sas->flags & 0x1C) {
	case 0:
		--sa->pauseTicksCount;
		if (sa->pauseTicksCount <= 0) {
			sa->frameIndex += sa->direction;
		}
		break;
	case 4:
		if (rnd < sas->len * 82) {
			sa->frameIndex += sa->direction;
		}
		break;
	case 8:
		--sa->pauseTicksCount;
		if (sa->pauseTicksCount <= 0) {
			if (sa->flags != 0) {
				sa->frameIndex += sa->direction;
			} else {
				sa->direction = -sa->direction;
				sa->frameIndex += sa->direction;
			}
		}
		break;
	case 12:
		if (sa->flags != 0) {
			sa->direction = sa->flags;
			sa->flags = 0;
		}
		sa->frameIndex = sas->len;
		break;
	case 16:
		if (sa->frame2Index != 0) {
			if (rnd < sas->len * 82) {
				sa->flags = sa->direction;
				sa->direction = 1;
				sa->prevFrame = sa->frameIndex;
				sa->frameIndex = sa->frame2Index;
			} else {
				sa->frameIndex += sa->direction;
			}
		} else {
			--sa->pauseTicksCount;
			if (sa->pauseTicksCount <= 0) {
				sa->frameIndex += sa->direction;
			}
		}
		break;
	case 20:
		--sa->pauseTicksCount;
		if (sa->pauseTicksCount <= 0) {
			if (sa->flags != 0) {
				sa->direction = sa->flags;
				sa->flags = 0;
				sa->frameIndex = sa->prevFrame + sa->direction;
			} else {
				sa->frameIndex = 0;
			}
		}
		break;
	case 24:
		--sa->pauseTicksCount;
		if (sa->pauseTicksCount <= 0) {
			sa->frameIndex += sa->direction;
		}
		break;
	}
	if (sa->frameIndex < 0) {
		sa->direction = -sa->direction;
		sa->frameIndex = 1;
	}
	if (sa->frameIndex >= sa->framesCount) {
		if (sa->flags != 0) {
			sa->direction = sa->flags;
			sa->flags = 0;
			sa->frameIndex = sa->prevFrame;
		} else {
			sa->frameIndex = 0;
		}
	}
	if (prevFrameIndex != sa->frameIndex) {
		sa->frmKey = _res.getChild(kResType_ANI, sa->aniKey);
		assert(sa->frmKey != 0);
		for (int i = 0; i < sa->frameIndex; ++i) {
			sa->frmKey = _res.getNext(kResType_ANI, sa->frmKey);
			assert(sa->frmKey != 0);
		}
		getSceneAnimationTexture(sa, &sas->len, &sas->flags, &_sceneAnimationsTextureTable[index]);
		sa->pauseTicksCount = sas->len;
		return false;
	}
	return true;
}

void Game::updateSceneAnimations() {
	const int rnd = _rnd.getRandomNumber();
	for (int i = 2; i < _sceneAnimationsCount2 + 2; ++i) {
		_sceneAnimationsTable[i].type &= ~0x40;
	}
	for (int i = 2; i < _sceneAnimationsCount2 + 2; ++i) {
		SceneAnimation *sa = &_sceneAnimationsTable[i];
		if ((sa->type & 0x20) != 0 || ((sa->type & 4) == 0 && (sa->type & 0x12) == 0x12)) {
			sa->type |= 0x50;
			int index = sa->next;
			while (index < _sceneAnimationsCount2 + 2 && index > 1 && (_sceneAnimationsTable[index].type & 0x40) == 0) {
				_sceneAnimationsTable[index].type |= 0x50;
				index = _sceneAnimationsTable[index].next;
			}
		}
	}
	for (int i = 2; i < _sceneAnimationsCount2 + 2; ++i) {
		SceneAnimation *sa = &_sceneAnimationsTable[i];
		SceneAnimationState *sas = &_sceneAnimationsStateTable[i];
		if (sa->aniKey == 0) {
			continue;
		}
		if ((sa->type & 0xC) == 0xC) {
			if (sa->frm2Key != 0) {
				sa->frmKey = _res.getChild(kResType_ANI, sa->frm2Key);
				assert(sa->frmKey != 0);
				sa->frm2Key = 0;
				const uint8_t *p_anikeyf = _res.getData(kResType_ANI, sa->frmKey, "ANIKEYF");
				assert(p_anikeyf);
				sa->ticksCount = p_anikeyf[0];
				getSceneAnimationTexture(sa, 0, 0, &_sceneAnimationsTextureTable[i]);
			} else if (sa->type & 2) {
				if (sa->ticksCount == 1) {
					int16_t nextKey = _res.getNext(kResType_ANI, sa->frmKey);
					if (nextKey == 0) {
						warning("Missing next ani frame %d", sa->frmKey);
						continue;
					}
					sa->frmKey = nextKey;
					const uint8_t *p_anikeyf = _res.getData(kResType_ANI, sa->frmKey, "ANIKEYF");
					assert(p_anikeyf);
					sa->ticksCount = p_anikeyf[0];
					getSceneAnimationTexture(sa, 0, 0, &_sceneAnimationsTextureTable[i]);
					nextKey = _res.getNext(kResType_ANI, sa->frmKey);
					if (nextKey == 0 && sa->ticksCount == 1) {
						sa->type = sa->typeInit;
						sa->frameNum = sa->frameNumInit | 0x20;
						sa->frameIndex = 0;
						sa->frmKey = _res.getChild(kResType_ANI, sa->aniKey);
					}
				} else {
					--sa->ticksCount;
					if (sa->ticksCount == 1) {
						int16_t nextKey = _res.getChild(kResType_ANI, sa->frmKey);
						if (nextKey == 0) {
							sa->type = sa->typeInit | 0x20;
							sa->frameNum = sa->frameNumInit;
						}
					}
				}
			}
		} else if ((sa->type & 0x12) == 0x12 || (sa->type & 0x20) != 0) {
			if ((sa->type & 1) == 0) {
				sa->type &= ~0x10;
			}
			sa->type &= ~0x20;
			bool needTextureupdate = true;
			if (sa->framesCount > 1 && sa->aniKey > 0) {
				needTextureupdate = updateSceneAnimationsKeyFrame(rnd, i, sa, sas);
			}
			if (needTextureupdate) {
				getSceneAnimationTexture(sa, 0, 0, &_sceneAnimationsTextureTable[i]);
			}
		}
	}
}

void Game::getSceneTexture(int16_t key, int framesSkip, SpriteImage *spr) {
	int16_t frameKey = _res.getChild(kResType_ANI, key);
	for (int i = 0; i < framesSkip; ++i) {
		frameKey = _res.getNext(kResType_ANI, frameKey);
	}
	frameKey = _res.getChild(kResType_ANI, frameKey);
	assert(frameKey != 0);
	const uint8_t *p_frm = _res.getData(kResType_ANI, frameKey, "ANIFRAM");
	int16_t sprKey = READ_LE_UINT16(p_frm);
	const uint8_t *p_btm = _res.getData(kResType_SPR, sprKey, "BTMDESC");
	spr->w = READ_LE_UINT16(p_btm);
	spr->h = READ_LE_UINT16(p_btm + 2);
	spr->data = _res.getData(kResType_SPR, sprKey, "SPRDATA");
	spr->key = sprKey;
}

void Game::loadSceneTextures(int16_t key) {
	int16_t texKey = _res.getChild(kResType_MAP, key);
	const uint8_t *p = _res.getData(kResType_MAP, texKey, "TEX3D");
	_sceneTexturesCount = READ_LE_UINT32(p);
	assert(_sceneTexturesCount < 256);

	debug(kDebug_GAME, "Game::loadSceneTextures() textures %d", _sceneTexturesCount);
	memset(_sceneTexturesTable, 0, sizeof(_sceneTexturesTable));
	p = _res.getData(kResType_MAP, texKey, "TEX3DANI");
	for (int i = 0; i < _sceneTexturesCount; ++i) {
		SceneTexture *st = &_sceneTexturesTable[i];
		st->framesCount = READ_LE_UINT32(p);
		st->key = READ_LE_UINT16(p + 4);
		p += 8;
		getSceneTexture(st->key, 0, &_sceneTextureImagesBuffer[i]);
		int16_t aniKey = _res.getChild(kResType_ANI, st->key);
		st->framesCount = 0;
		do {
			++st->framesCount;
			aniKey = _res.getNext(kResType_ANI, aniKey);
		} while (aniKey > 0);
	}
}

void Game::updateSceneTextures() {
	for (int i = 0; i < _sceneTexturesCount; ++i) {
		SceneTexture *st = &_sceneTexturesTable[i];
		getSceneTexture(st->key, (_ticks & 0xFFFF) % st->framesCount, &_sceneTextureImagesBuffer[i]);
	}
}

void Game::initScene() {
	loadSceneMap(_mapKey);
	GameObject *o = _objectsPtrTable[kObjPtrConrad];
	o->room = getCellMapShr19(o->xPos, o->zPos)->room;
	debug(kDebug_GAME, "Game::initScene() initial room %d", o->room);
	_roomsTable[o->room].fl = 1;
	loadSceneTextures(_mapKey);
	fixRoomData();
	_rayCastCounter = 0;
	if (_updatePalette) {
		_updatePalette = false;
		updatePalette();
	}
}

void Game::init() {
	debug(kDebug_GAME, "Game::init()");

	_res.loadTrigo();

	int dataSize;
	File *fp = fileOpen("PLAYER.INI", &dataSize, kFileType_DATA);
	_res.loadINI(fp, dataSize);
	fileClose(fp);

	_snd.init();

	_ticks = 0;
	_level = _params.levelNum;
	initLevel();
}

void Game::initLevel() {
	debug(kDebug_GAME, "Game::initLevel() %d\n", _level);

	int32_t flag = -1;
	op_clearTarget(1, &flag);
	int32_t argv[] = { -1, -1 };
	op_removeObjectMessage(2, argv);

	clearLevelData();

	if (_params.playDemo) {
		int dataSize;
		char name[16];
		snprintf(name, sizeof(name), "LEVEL%d.DEM", _level);
		File *fp = fileOpen(name, &dataSize, kFileType_DATA, false);
		if (fp) {
			_res.loadDEM(fp, dataSize);
			fileClose(fp);
		}
	}
	_demoInput = 0;

	_snd._musicMode = 1;
	_snd._musicKey = 0;

	_varsTable[kVarPlayerObject] = 0;
	_inputsTable = 0;
	_inputsCount = 0;

	_room = 0;
	_updatePalette = false;
	memset(_screenPalette, 0, sizeof(_screenPalette));
	memset(_indirectPalette, 0, sizeof(_indirectPalette));
	memset(_mrkBuffer, 0, sizeof(_mrkBuffer));
	_particlesCount = 0;

	_mainLoopCurrentMode = 0;
	_conradHit = 0;
	_viewportSize = kViewportMax;
	_newPlayerObject = 0;

	clearGlobalData();
	_varsTable[kVarConradLife] = 2000;
	_res.loadLevelData(_res._levelDescriptionsTable[_level].name, _level + 1);
	_mapKey = _res.getKeyFromPath(_res._levelDescriptionsTable[_level].mapKey);
	getAllPalKeys(_mapKey);
	for (int i = 0; i < kSoundKeyPathsTableSize; ++i) {
		_res._sndKeysTable[i] = _res.getKeyFromPath(_res._soundKeyPathsTable[i]);
	}
	setupObjects();
	initFonts();
	setupInventoryObjects();
	initSprites();
	initScene();

	setupConradObject();
	GameObject *o = getObjectByKey(_cameraViewKey);
	changeRoom(o->room);
	_roomPrev = _room = o->room;
	_endGame = false;
	clearKeyboardInput();
	_objectsPtrTable[kObjPtrMusic] = 0;
	_snd._musicMode = 1;
	playMusic(-1);

//	_focalDistance = 0;
	_currentObject = _objectsPtrTable[kObjPtrConrad];
//	_xPosViewpoint = _x0PosViewpoint = _currentObject->xPosParent + _currentObject->xPos;
//	_yPosViewpoint = _y0PosViewpoint = _currentObject->yPosParent + _currentObject->yPos;
//	_zPosViewpoint = _z0PosViewpoint = _currentObject->zPosParent + _currentObject->zPos;
//	_cameraState = 0;
//	_cameraViewKey = _currentObject->objKey;
	_currentObject->specialData[1][18] = 2000;
//	setCameraObject(_currentObject, &_cameraViewObj);
//	_varsTable[31] = _cameraViewKey;
}

void Game::setupConradObject() {
	GameObject *o = getObjectByKey(_conradObjectKey);
	_objectsPtrTable[12] = _objectsPtrTable[13] = o;
	_cameraViewKey = o->objKey;
	_cameraViewObj = 1;
	_varsTable[31] = _cameraViewKey;
}

void Game::changeRoom(int room) {
	debug(kDebug_GAME, "Game::changeRoom() room %d/%d", _room, room);
	if (_roomsTable[_room].palKey != _roomsTable[room].palKey) {
		_updatePalette = true;
	}
}

void Game::playMusic(int mode) {
	if (_objectsPtrTable[kObjPtrConrad]->room <= 0) {
		return;
	}
	if (mode < 0) {
		mode = -mode;
	}
	int16_t currentKey = _snd._musicKey;
	if (mode == 1) {
		const int num = _roomsTable[_objectsPtrTable[kObjPtrConrad]->room].o->customData[mode];
		_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[num];
	} else if (mode == 3) {
		if (_objectsPtrTable[kObjPtrMusic]->specialData[1][21] == 67108864) {
			switch (_objectsPtrTable[kObjPtrMusic]->specialData[1][22]) {
			case 1:
				_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[10];
				break;
			case 2:
				_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[11];
				break;
			}
		} else if (_objectsPtrTable[kObjPtrMusic]->specialData[1][21] == 0x4000) {
			_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[12];
		} else {
			switch (_objectsPtrTable[kObjPtrMusic]->specialData[1][22]) {
			case 0:
				break;
			case 1:
			case 262144:
			case 524288:
			case 2:
			case 8192:
			case 16384:
				if (_level != 8 && _level != 9) {
					if (_ticks & 1) {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[7];
					} else {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[8];
					}
				}
				break;
			case 4:
				_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[9];
				break;
			case 1073741824:
				_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[12];
				break;
			case 8:
				break;
			case 2097152:
			case 128:
			case 256:
			case 512:
				if (_level == 8) {
					break;
				}
				if (_level == 2 || _level == 3 || _level == 4 || _level == 6 || _level == 7 || _level == 9 || _level == 11) {
					if (_objectsPtrTable[kObjPtrMusic]->specialData[1][22] == 2097152) {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[6];
					}
				} else if (_level != 9) {
					if (_ticks & 2) {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[4];
					} else {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[5];
					}
				} else {
					if (_ticks & 2) {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[4];
					} else if ((_ticks & 3) == 3) {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[5];
					} else {
						_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[6];
					}
				}
				break;
			case 32768:
				if (_level == 1) {
					break;
				}
			case 16:
			case 32:
			case 1024:
			case 2048:
			case 4096:
			case 131072:
				if (_level != 8 && _level != 9) {
					_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[5];
				}
				break; 
			case 65536:
				break;
			default:
				if (_level != 8 && _level != 9) {
					_snd._musicKey = _res._levelDescriptionsTable[_level].musicKeys[5];
				}
				break;
			}
		}
	}
	if (currentKey != _snd._musicKey) {
		if (currentKey > 0) {
			_snd.stopMidi(_objectsPtrTable[kObjPtrWorld]->objKey, currentKey);
		}
		if (_snd._musicKey > 0) {
			_snd.playMidi(_objectsPtrTable[kObjPtrWorld]->objKey, _snd._musicKey);
			const uint8_t *p_sndtype = _res.getData(kResType_SND, _snd._musicKey, "SNDTYPE");
			if (p_sndtype) {
				_snd._musicMode = mode;
			}
		}
	}
}

void Game::clearObjectMessage(GameObject *o) {
	GameMessage *m_cur = o->msg;
	while (m_cur) {
		GameMessage *m_next = m_cur->next;
		free(m_cur);
		m_cur = m_next;
	}
	o->msg = 0;
}

int Game::isScriptAnimFrameEnd() {
	return _currentObject->anim.anikeyfData == 0;
}

void Game::setObjectData(GameObject *o, int param, int32_t value) {
	if (param >= 256) {
		switch (param) {
		case 257:
			if (o != _currentObject) {
				if (!o->updateColliding && !o->setColliding) {
					addToCollidingObjects(o);
				}
			}
			o->xPos = value << kPosShift;
			break;
		case 258:
			if (o != _currentObject) {
				if (!o->updateColliding && !o->setColliding) {
					addToCollidingObjects(o);
				}
			}
			o->yPos = value << kPosShift;
			break;
		case 259:
			if (o != _currentObject) {
				if (!o->updateColliding && !o->setColliding) {
					addToCollidingObjects(o);
				}
			}
			o->zPos = value << kPosShift;
			break;
		case 267:
			if (!(value == 3 && o->state == 1) && !(value == 2 && o->state == 0)) {
				o->state = value;
				if (value >= 2 && value <= 6) {
					addToChangedObjects(o);
				}
			}
			break;
		case 268:
			o->pitch = value;
			break;
		case 274:
			if (o != _currentObject) {
				if (!o->updateColliding && !o->setColliding) {
					addToCollidingObjects(o);
				}
			}
			o->xPos = value;
			break;
		case 275:
			if (o != _currentObject) {
				if (!o->updateColliding && !o->setColliding) {
					addToCollidingObjects(o);
				}
			}
			o->zPos = value;
			break;
		case 277:
			if (o != _currentObject) {
				if (!o->updateColliding && !o->setColliding) {
					addToCollidingObjects(o);
				}
			}
			o->yPos = value;
			break;
		default:
			error("Game::setObjectData() unhandled param %d", param);
			break;
		}
	} else if (param >= 128) {
		error("Game::setObjectData() invalid access to _varsTable");
	} else if (param >= 96) {
		setObjectFlag(o, param, value);
	} else if (param >= 64) {
		param -= 64;
		assert(param < 12);
		o->customData[param] = value;
	} else {
		assert(param <= 26);
		if (param == 8) {
			if (!o->updateColliding && !o->setColliding) {
				addToCollidingObjects(o);
			}
		}
		o->specialData[1][param] = value;
	}
}

int32_t Game::getObjectData(GameObject *o, int param) {
	int32_t value = 0;
	if (param >= 256) {
		switch (param) {
		case 257:
			value = o->xPos;
			break;
		case 258:
			value = o->yPos;
			break;
		case 259:
			value = o->zPos;
			break;
		case 261:
			value = o->objKey;
			break;
		case 267:
			value = o->state;
			break;
		case 268:
			value = o->pitch;
			break;
		case 269:
			value = o->o_parent->objKey;
			break;
		case 270:
			if (o->o_child) {
				value = o->o_child->objKey;
			}
			break;
		case 271:
			value = o->xPosParent + o->xPos;
			break;
		case 273:
			value = o->zPosParent + o->zPos;
			break;
		default:
			error("Game::getObjectData() unhandled param %d", param);
			break;
		}
	} else if (param >= 128) {
		error("Game::getObjectData() invalid access to _varsTable");
	} else if (param >= 96) {
		param -= 96;
		value = getObjectFlag(o, param);
	} else {
		value = o->getData(param);
	}
	return value;
}

int32_t Game::getObjectScriptParam(GameObject *o, int param) {
	int32_t value = 0;
	if (param >= 256) {
		switch (param) {
		case 257:
			value = o->xPos;
			break;
		case 258:
			value = o->yPos;
			break;
		case 259:
			value = o->zPos;
			break;
		case 260:
			value = o->room;
			break;
		case 261:
			value = o->objKey;
			break;
		case 268:
			value = o->pitch;
			break;
		case 269:
			value = o->o_parent->objKey;
			break;
		case 270:
			if (o->o_child) {
				value = o->o_child->objKey;
			}
			break;
		case 271:
			value = o->xPosParent + o->xPos;
			break;
		case 273:
			value = o->zPosParent + o->zPos;
			break;
		default:
			error("Game::getObjectScriptParam() unhandled param %d", param);
			break;
		}
	} else if (param >= 128) {
		param -= 128;
		value = _varsTable[param];
	} else if (param >= 96) {
		param -= 96;
		value = getObjectFlag(o, param);
	} else {
		value = o->getData(param);
		if (param == 20) {
			value &= 15;
		}
	}
	return value;
}

static const uint8_t _opcodeSize[kOpcodesCount] = {
	0, 2, 3, 2, 2, 6, 2, 3, 3, 2, 0, 2, 3, 4, 4, 0,
	0, 0, 0, 1, 4, 1, 5, 6, 1, 1, 2, 2, 0, 3, 1, 1,
	2, 4, 4, 5, 4, 3, 2, 4, 3, 2, 0, 2, 1, 1, 1, 0,
	2, 3, 2, 4, 3, 3, 4, 1, 0, 2, 4, 5, 5, 1, 4, 3,
	6, 4, 2, 3, 2, 7, 3, 4, 1, 4, 1, 0, 2, 2, 1, 1
};

static const uint8_t _opcodeSize_demo[kOpcodesCount] = {
	0, 2, 3, 2, 2, 6, 2, 3, 3, 1, 0, 2, 3, 4, 4, 0,
	0, 0, 0, 1, 4, 1, 5, 6, 1, 1, 2, 2, 0, 3, 1, 1,
	2, 4, 4, 5, 4, 3, 2, 4, 3, 2, 0, 2, 1, 1, 1, 0,
	2, 3, 2, 4, 3, 3, 4, 1, 0, 2, 4, 5, 5, 1, 4, 3,
	6, 4, 2, 3, 2, 7, 3, 4, 1, 4, 1, 0, 2, 2, 1, 0
};

static const Game::OpcodeProc _opcodeTable[kOpcodesCount] = {
	// 0
	&Game::op_true,
	&Game::op_toggleInput,
	&Game::op_compareCamera,
	&Game::op_sendMessage,
	// 4
	&Game::op_getObjectMessage,
	&Game::op_setParticleParams,
	&Game::op_setVar,
	&Game::op_compareConst,
	// 8
	&Game::op_evalVar,
	&Game::NULLED,
	&Game::NULLED, // &Game::op_isCurrentObjectInDrawList, // unused
	&Game::op_getAngle,
	// 12
	&Game::op_setObjectData,
	&Game::op_evalObjectData,
	&Game::op_compareObjectData,
	0, // &Game::op_enterMenuSaveGame, // unused
	// 16
	0, // &Game::op_enterMenuLoadGame, // unused
	&Game::op_jumpToNextLevel,
	0, // &Game::op_quitGame, // unused
	&Game::op_rand,
	// 20
	&Game::op_isObjectColliding,
	&Game::op_sendShootMessage,
	&Game::op_getShootInfo,
	&Game::op_updateTarget,
	// 24
	&Game::op_moveObjectToObject,
	&Game::op_getObject9,
	&Game::op_setObjectParent,
	&Game::op_removeObjectMessage,
	// 28
	&Game::op_setBoxItem,
	&Game::NULLED, // &Game::op_fadePalette, // unused
	&Game::op_isObjectMoving,
	&Game::op_getMessageInfo,
	// 32
	&Game::op_testObjectsRoom,
	&Game::op_setCellMapData,
	&Game::op_addCellMapData,
	&Game::op_compareCellMapData,
	// 36
	&Game::op_getObjectDistance,
	&Game::op_setObjectSpecialCustomData,
	&Game::op_transformObjectPos,
	&Game::op_compareObjectAngle,
	// 40
	&Game::op_moveObjectToPos,
	&Game::op_setupObjectPath,
	&Game::op_continueObjectMove,
	&Game::op_compareInput,
	// 44
	&Game::op_clearTarget,
	&Game::op_playCutscene,
	&Game::op_setScriptCond,
	&Game::op_isObjectMessageNull,
	// 48
	&Game::op_detachObjectChild,
	&Game::op_setCamera,
	&Game::op_getSquareDistance,
	&Game::op_isObjectTarget,
	// 52
	&Game::op_printDebug,
	&Game::op_translateObject,
	&Game::op_setupTarget,
	&Game::op_getTicks,
	// 56
	&Game::op_swapFrameXZ,
	&Game::op_addObjectMessage,
	&Game::op_setupCircularMove,
	&Game::op_moveObjectOnCircle,
	// 60
	&Game::op_isObjectCollidingType,
	&Game::op_setLevelData,
	&Game::op_drawNumber,
	&Game::op_isObjectOnMap,
	// 64
	&Game::NULLED, // &Game::op_isCollidingLine, // unused
	&Game::op_updateFollowingObject,
	&Game::NULLED, // &Game::op_rotateCoords, // unused
	&Game::op_translateObject2,
	// 68
	&Game::op_updateCollidingHorizontalMask,
	&Game::op_createParticle,
	&Game::op_setupFollowingObject,
	&Game::op_isObjectCollidingPos,
	// 72
	&Game::op_setCameraObject,
	&Game::op_setCameraParams,
	&Game::op_setPlayerObject,
	&Game::op_isCollidingRooms,
	// 76
	&Game::op_isMessageOnScreen,
	&Game::op_debugBreakpoint,
	&Game::op_isObjectConradNotVisible,
	&Game::op_stopSound
};

int Game::executeObjectScriptOpcode(GameObject *o, uint32_t op, const uint8_t *data) {
	int32_t val, argv[8];

	debug(kDebug_GAME, "Game::executeObjectScriptOpcode() o %p op %d", o, op);
	assert(op < kOpcodesCount);
	const int argc = g_isDemo ? _opcodeSize_demo[op] : _opcodeSize[op];
	assert(argc <= 8);
	uint32_t mask = READ_LE_UINT32(data); data += 4;
	for (int i = 0; i < argc; ++i) {
		val = READ_LE_UINT32(data); data += 4;
		if (mask & (1 << i)) {
			val = getObjectScriptParam(o, val);
		}
		argv[i] = val;
	}
	if (op >= kOpcodesCount || !_opcodeTable[op]) {
		warning("Game::executeObjectScriptOpcode() invalid opcode %d", op);
	}
	return (this->*_opcodeTable[op])(argc, argv);
}

uint8_t *Game::getStartScriptAnim() {
	uint8_t *p = 0;
	if (_currentObject->scriptStateKey > 0) {
		_currentScriptKey = _res.getChild(kResType_STM, _currentObject->scriptStateKey);
		if (_currentScriptKey != 0) {
			assert(_currentScriptKey > 0);
			p = _res.getData(kResType_STM, _currentScriptKey, "STMCOND");
		}
		_currentObject->scriptCondKey = _currentScriptKey;
	}
	return p;
}

uint8_t *Game::getNextScriptAnim() {
	uint8_t *p = 0;
	if (_currentScriptKey > 0) {
		_currentScriptKey = _res.getNext(kResType_STM, _currentScriptKey);
		if (_currentScriptKey != 0) {
			assert(_currentScriptKey > 0);
			p = _res.getData(kResType_STM, _currentScriptKey, "STMCOND");
		}
		_currentObject->scriptCondKey = _currentScriptKey;
	}
	return p;
}

int Game::executeObjectScript(GameObject *o) {
	int runScript = 0;
	int currentInput = getCurrentInput();
	uint8_t inputKey0 = _inputsTable[currentInput].inputKey0;
	_currentObject = o;
//	objKey = o->objKey;
	int scriptMsgNum = -1;
	if (o->msg) {
		int messagesCount = o->scriptStateData[3];
		if (messagesCount != 0) {
			const uint8_t *msgList = _res.getMsgData(o->scriptStateData[2]);
			for (int i = 0; i < messagesCount && runScript == 0; ++i) {
				if (msgList[i] == 0) {
					break;
				}
				for (GameMessage *objectMsg = o->msg; objectMsg && runScript == 0; objectMsg = objectMsg->next) {
					if (objectMsg->num == msgList[i]) {
						scriptMsgNum = objectMsg->num;
						runScript = 1;
					}
				}
			}
			if (scriptMsgNum == 58 && o->specialData[1][18] <= 0) {
				o->specialData[1][23] = 0;
			}
		}
	}
	if (o->anim.aniheadData) {
		if (o == _objectsPtrTable[kObjPtrConrad] && o->objKey == _varsTable[kVarPlayerObject]) {
			if ((_inputDirKeyReleased[inputKey0] & o->specialData[1][19]) != 0) {
				runScript = 1;
			}
		}
		const uint32_t inputMask = _inputDirKeyPressed[inputKey0] | (_inputButtonKey[inputKey0] << 4);
		if (READ_LE_UINT16(o->anim.aniheadData + 2) & inputMask) {
			runScript = 1;
		}
		if (o->specialData[1][21] == 2 && o->objKey == _varsTable[kVarPlayerObject]) {
			if (READ_LE_UINT16(o->anim.aniheadData + 4) != 0) {
				const int rotationStep = 16 + _varsTable[26] / 2;
				int objectRy = 0;
				if (_inputDirKeyPressed[inputKey0] & 2) {
					objectRy = -rotationStep;
				} else if (_inputDirKeyPressed[inputKey0] & 1) {
					objectRy = rotationStep;
				}
				_varsTable[26] = 0;
				if (objectRy != 0) {
					o->pitch += objectRy;
					o->pitch &= 1023;
					if (_varsTable[26] < 1024 / 16) {
						_varsTable[26] += 3;
					}
				}
			} else {
				_varsTable[26] = 0;
			}
		}
	}
	if (runScript == 0) {
		if (gotoNextScriptAnim() == 1) {
			return 0;
		}
	} else if (o->specialData[1][23] == scriptMsgNum) {
	} else {
		const int fl = (scriptMsgNum == 57) ? 1 : 0;
		gotoEndScriptAnim(fl);
	}
	if (runScript || isScriptAnimFrameEnd()) {
		int stopScript = 0;
		_currentObject->scriptCondData = getStartScriptAnim();
		while (_currentObject->scriptCondData && !stopScript) {
			const int scriptCmdNum = READ_LE_UINT16(_currentObject->scriptCondData);
			debug(kDebug_OPCODES, "scriptCmdNum=%d object='%s' key=%d", scriptCmdNum, _currentObject->name, _currentObject->objKey);
			const uint8_t *scriptData = _res.getCmdData(scriptCmdNum);
			int scriptRet = 1;
			while (scriptRet) {
				uint32_t op = READ_LE_UINT32(scriptData); scriptData += 4;
				if (op == 0xFFFFFFFF) {
					// end of conditions sequence
					break;
				}
				bool negateScriptRet = false;
				if (op & 0x80) {
					negateScriptRet = true;
					op &= ~0x80;
				}
				scriptRet = executeObjectScriptOpcode(_currentObject, op, scriptData);
				if (negateScriptRet) {
					scriptRet = ~scriptRet;
				}
				const int count = g_isDemo ? _opcodeSize_demo[op] : _opcodeSize[op];
				scriptData += count * 4 + 4;
			}
			if (scriptRet) {
				stopScript = 1;
				while (1) {
					uint32_t op = READ_LE_UINT32(scriptData); scriptData += 4;
					if (op == 0xFFFFFFFE) {
						// end of statements sequence
						break;
					}
					executeObjectScriptOpcode(_currentObject, op, scriptData);
					const int count = g_isDemo ? _opcodeSize_demo[op] : _opcodeSize[op];
					scriptData += count * 4 + 4;
				}
			}
			if (!stopScript) {
				_currentObject->scriptCondData = getNextScriptAnim();
			}
		}
		clearObjectMessage(_currentObject);
		if (_endGame) {
			return 0;
		}
		if (!stopScript) {
			gotoStartScriptAnim();
			if (isScriptAnimFrameEnd()) {
				return 0;
			}
		} else {
			if (runScript && scriptMsgNum != -1 && o->specialData[1][23] == scriptMsgNum && o->anim.anikeyfData) {
				if (gotoNextScriptAnim() == 1 || isScriptAnimFrameEnd()) {
					return 0;
				}
			} else {
				int16_t scriptKey = o->scriptStateKey;
				o->scriptStateKey = READ_LE_UINT16(o->scriptCondData + 4);
				const uint8_t *scriptData = o->scriptStateData;
				o->scriptStateData = _res.getData(kResType_STM, o->scriptStateKey, "STMSTATE");
				if (gotoStartScriptAnim() == 1 || isScriptAnimFrameEnd()) {
					o->scriptStateKey = scriptKey;
					o->scriptStateData = scriptData;
					return 0;
				}
			}
		}
	}
	if (_currentObject->anim.anikeyfData == 0) {
		_currentObject->anim.anikeyfData = _res.getData(kResType_ANI, _currentObject->anim.currentAnimKey, "ANIKEYF");
	}
	return 1;
}

void Game::runObject(GameObject *o) {
	int x, y, z, ry;
	do {
//printf("runobject name '%s' state %d pos %d,%d %d %d decor %d\n", o->name, o->state, o->xPos >> 19, o->yPos >> 19, o->zPos >> 19, o->pitch, (o->flags[1] & 0x100) ? 1 : 0);
		if (o->state == 1 && (o->flags[1] & 0x4000) == 0) {
			o->xPosParentPrev = o->xPosParent;
			o->yPosParentPrev = o->yPosParent;
			o->zPosParentPrev = o->zPosParent;
			GameObject *o_parent = o->o_parent;
			o->xPosParent = o_parent->xPosParent + o_parent->xPos;
			o->yPosParent = o_parent->yPosParent + o_parent->yPos;
			o->zPosParent = o_parent->zPosParent + o_parent->zPos;
			if (o->setColliding) {
				x = o->xPosPrev;
				y = o->yPosPrev;
				z = o->zPosPrev;
				ry = o->pitchPrev;
			} else {
				x = o->xPos;
				y = o->yPos;
				z = o->zPos;
				ry = o->pitch;
			}
			int scriptExecCount = 0;
			for (; scriptExecCount < 10 && !_endGame; ++scriptExecCount) {
				if (executeObjectScript(o) != 0) {
					break;
				}
			}
			if (scriptExecCount == 10) {
				warning("Game::executeObjectScript() possible infinite script loop for object '%s'", o->name);
				o->state = 0;
			}
			o->xPosPrev = x;
			o->yPosPrev = y;
			o->zPosPrev = z;
			o->pitchPrev = ry;
			if (o->xPos != o->xPosPrev || o->zPos != o->zPosPrev || o->o_parent->updateColliding || o->parentChanged) {
				resetCollisionSlot(o);
				setCollisionSlotsUsingCallback1(o->xPosParent + o->xPos, o->zPosParent + o->zPos, &Game::collisionSlotCb2);
				o->updateColliding = true;
				o->parentChanged = false;
				o->room = getCellMapShr19(o->xPosParent + o->xPos, o->zPosParent + o->zPos)->room;
			}
			o->inSceneList = 0;
			GameObject *o_child = o->o_child;
			if (o_child && (o_child->flags[1] & 0x100) == 0) {
				runObject(o_child);
			}
		} else if (o->o_parent->updateColliding) {
			if (!o->setColliding) {
				o->setColliding = true;
				addToCollidingObjects(o);
			}
		}
		o->updateColliding = false;
		assert(o != o->o_next);
//		if (o == o->o_next) {
//			o->o_next = 0;
//		}
	} while ((o = o->o_next) != 0 && (o->flags[1] & 0x100) == 0);
}

void Game::setplayerRoomObjectsData(int fl) {
	GameObject *o_player = getObjectByKey(_varsTable[kVarPlayerObject]);
	int xPos = o_player->xPos + o_player->xPosParent;
	int zPos = o_player->zPos + o_player->zPosParent;
	CellMap *cell = getCellMapShr19(xPos, zPos);
	GameObject *o = _roomsTable[cell->room].o;
	assert(o);
	while (o != _objectsPtrTable[kObjPtrWorld]) {
		setObjectData(o, 267, fl);
		o = o->o_parent;
	}
	if (cell->room2 != 0) {
		o = _roomsTable[cell->room2].o;
		assert(o);
		while (o != _objectsPtrTable[kObjPtrWorld]) {
			setObjectData(o, 267, fl);
			o = o->o_parent;
		}
	}
}

void Game::setPlayerObject(int16_t objKey) {
	GameObject *o = (objKey == 0) ? _currentObject : _objectsPtrTable[objKey];
	assert(o);
	assert((o->flags[1] & 0x80) == 0 && (o->flags[1] & 4) == 0 && (o->flags[1] & 0x10000) != 0);
	assert(o->state == 1);
	setplayerRoomObjectsData(0);
	_varsTable[kVarPlayerObject] = o->objKey;
	debug(kDebug_GAME, "Game::setPlayerObject() _varsTable[kVarPlayerObject] %d", _varsTable[kVarPlayerObject]);
	setplayerRoomObjectsData(1);
	setCameraObject(o, &_cameraViewObj);
	for (int i = 0; i < _inputsCount; ++i) {
		GameInput *inp = &_inputsTable[i];
		inp->keymaskPrev = inp->keymask = 0;
		inp->sysmaskPrev = inp->sysmask = 0;
		_inputDirKeyReleased[inp->inputKey0] = _inputDirKeyReleased[inp->inputKey1] = 0;
		_inputDirKeyPressed[inp->inputKey0] = _inputDirKeyPressed[inp->inputKey1] = 0;
		_inputButtonKey[inp->inputKey0] = _inputButtonKey[inp->inputKey1] = 0;
	}
}

void Game::updatePlayerObject() {
	GameObject *o;

	if (_newPlayerObject != 0) {
		o = getObjectByKey(_newPlayerObject);
		assert(o);
		assert((o->flags[1] & 0x80) == 0 && (o->flags[1] & 4) == 0 && (o->flags[1] & 0x10000) != 0);
		setPlayerObject(_newPlayerObject);
		_newPlayerObject = 0;
	} else {
		int16_t objKey = _varsTable[kVarPlayerObject];
		if (_objectsPtrTable[kObjPtrConrad]->objKey != objKey) {
			o = getObjectByKey(objKey);
			if (o) {
				while (o->state == 1 && o != _objectsPtrTable[kObjPtrWorld]) {
					o = o->o_parent;
				}
				if (o != _objectsPtrTable[kObjPtrWorld]) {
					setPlayerObject(_objectsPtrTable[kObjPtrConrad]->objKey);
				}
			} else {
				setPlayerObject(_objectsPtrTable[kObjPtrConrad]->objKey);
			}
		}
	}
}

void Game::addToChangedObjects(GameObject *o) {
	assert(_changedObjectsCount < kChangedObjectsTableSize);
	for (int i = 0; i < _changedObjectsCount; ++i) {
		if (_changedObjectsTable[_changedObjectsCount] == o) {
			return;
		}
	}
	_changedObjectsTable[_changedObjectsCount] = o;
	++_changedObjectsCount;
}

void Game::updateChangedObjects() {
	for (int i = 0; i < _changedObjectsCount; ++i) {
		GameObject *o = _changedObjectsTable[i];
		assert(o);
		switch (o->state) {
		case 2:
			o->state = 0;
			if (o->specialData[1][21] == 16) {
				playMusic(1);
			}
			break;
		case 3:
			o->state = 1;
			break;
		case 4:
			_currentObject = o;
			resetCollisionSlot(o);
			if (_objectsPtrTable[kObjPtrTarget]->o_parent == o) {
				_varsTable[16] = 0;
			}
			setObjectParent(o, _objectsPtrTable[kObjPtrCimetiere]);
			if (o->specialData[1][21] == 16) {
				playMusic(1);
			}
			{
				int32_t args[2] = { o->objKey, -1 };
				op_removeObjectMessage(2, args);
			}
			break;
		case 5:
			_currentObject = o;
			resetCollisionSlot(o);
			o->specialData[1][8] = 0;
			setObjectParent(o, getObjectByKey(o->customData[0]));
			for (int i = 0; i < 26; ++i) {
				o->specialData[1][i] = o->specialData[0][i];
			}
			o->state = 0;
			break;
		case 6:
			_currentObject = o;
			resetCollisionSlot(o);
			o->specialData[1][8] = 0;
			setObjectParent(o, getObjectByKey(o->customData[0]));
			for (int i = 0; i < 26; ++i) {
				o->specialData[1][i] = o->specialData[0][i];
			}
			o->state = 1;
			break;
		}
	}
	if (_snd._musicMode != 1) {
		GameObject *o_tmp, *o_music = _objectsPtrTable[kObjPtrMusic];
		if ((o_music->flags[1] & 0x80) != 0 && o_music->specialData[1][22] == 0x40000) {
			o_tmp = o_music;
		} else {
			o_music->room = getCellMapShr19(o_music->xPos + o_music->xPosParent, o_music->zPos + o_music->zPosParent)->room;
			o_tmp = _roomsTable[o_music->room].o;
		}
		if (o_tmp) {
			while (o_tmp->state == 1 && o_tmp != _objectsPtrTable[kObjPtrWorld]) {
				o_tmp = o_tmp->o_parent;
			}
		}
		if (o_tmp != _objectsPtrTable[kObjPtrWorld]) {
			playMusic(-1);
		}
	}
	if (_cameraViewKey != _objectsPtrTable[kObjPtrConrad]->objKey) {
		GameObject *o = getObjectByKey(_cameraViewKey);
		if (o) {
			while (o->state == 1 && o != _objectsPtrTable[kObjPtrWorld]) {
				o = o->o_parent;
			}
		}
		if (o != _objectsPtrTable[kObjPtrWorld]) {
			setCameraObject(_objectsPtrTable[kObjPtrConrad], &_cameraViewObj);
		}
	}
	_changedObjectsCount = 0;
}

void Game::updateCameraViewpoint(int xPos, int yPos, int zPos) {
	static const int e = 1 << (kPosShift - 1);
	if (xPos < _xPosViewpoint - e || xPos > _xPosViewpoint + e) {
		_cameraDx = (((-_xPosViewpoint + xPos) / 5) + _cameraDx) / 2;
	} else {
		_cameraDx = 0;
	}
	_xPosViewpoint += _cameraDx;
	if (yPos < _yPosViewpoint - e || yPos > _yPosViewpoint + e) {
		_cameraDy = (((-_yPosViewpoint + yPos) / 5) + _cameraDy) / 2;
	} else {
		_cameraDy = 0;
	}
	_yPosViewpoint += _cameraDy;
	if (zPos < _zPosViewpoint - e || zPos > _zPosViewpoint + e) {
		_cameraDz = (((-_zPosViewpoint + zPos) / 5) + _cameraDz) / 2;
	} else {
		_cameraDz = 0;
	}
	_zPosViewpoint += _cameraDz;
}

bool Game::updateGlobalPos(int dx, int dy, int dz, int dx0, int dz0, int flag) {
	bool ret = true;
	GameObject *o = _currentObject;
	int pitchTable[3] = { 0, 0, 0 };
	int angle = 2;
	bool collidingTest = false;
	CollisionSlot2 slots2[65];
	if ((o->flags[1] & 0x10000) != 0 && _varsTable[kVarPlayerObject] == o->objKey) {
		pitchTable[0] = 0;
		pitchTable[1] = 48;
		pitchTable[2] = -48;
		angle = 0;
	}
	_updateGlobalPosRefObject = 0;
	int rx, rz, rx0, rz0;
	int cosy, siny;
	int x, y, z;
	do {
		slots2[0].box = -1;
		const int a = (o->pitch + pitchTable[angle]) & 1023;
		cosy = g_cos[a];
		siny = g_sin[a];
		rx0 = cosy * dx0 - siny * dz0;
		rz0 = siny * dx0 + cosy * dz0;
		if (flag == 0) {
			rx0 >>= 1;
			rz0 >>= 1;
		} else {
			rx0 >>= 5;
			rz0 >>= 5;
		}
		rx = (cosy * dx - siny * dz) >> 5;
		rz = (siny * dx + cosy * dz) >> 5;
		x = o->xPosParent + o->xPos - rx;
		y = o->yPosParent + o->yPos + dy;
		z = o->zPosParent + o->zPos + rz;
		++angle;
		if (o->xPos - rx0 == o->xPosPrev && o->zPos + rz0 == o->zPosPrev && o->xPosParent == o->xPosParentPrev && o->zPosParent == o->zPosParentPrev) {
			collidingTest = true;
			break;
		}
		_updateGlobalPosRefObject = 0;
		collidingTest = setCollisionSlotsUsingCallback2(o, o->xPosParent + o->xPos - rx0, o->zPosParent + o->zPos + rz0, &Game::collisionSlotCb3, ~1, slots2);
	} while (!collidingTest && angle < 3);
	int roomPrev = o->room;
	o->xPos -= rx0;
	o->zPos += rz0;
	o->yPos += dy << 11;
	if ((o->flags[1] & 0x10000) != 0 && _varsTable[kVarPlayerObject] == o->objKey && collidingTest) {
		o->pitch += pitchTable[angle - 1];
	}
	if (collidingTest) {
		o->xPosWorld = x;
		o->yPosWorld = y - (((int16_t)READ_LE_UINT16(o->anim.anikeyfData + 4)) << 11);
		o->zPosWorld = z;
		x = o->xPosParent + o->xPos;
		z = o->zPosParent + o->zPos;
		CellMap *cell = getCellMapShr19(x, z);
		o->room = cell->room;
		if (o->room == 0) {
			o->room = cell->room2;
		}
		if (o->room == 0 || _roomsTable[o->room].o == 0) {
			warning("Game::updateGlobalPos() no room for object pos %d,%d (room %d room2 %d) name '%s'", x >> 19, z >> 19, cell->room, cell->room2, o->name);
			return ret;
		}
		assert(o->room != 0);
		if (o != _objectsPtrTable[kObjPtrConrad] && (o->flags[1] & 0x1000) != 0) {
			GameRoom *room = &_roomsTable[o->room];
			if (room->o->state != 1 && room->o->state != 3) {
				setObjectParent(o, room->o);
			} else if (testObjectsRoom(o->objKey, _objectsPtrTable[kObjPtrConrad]->objKey)) {
				setObjectParent(o, _roomsTable[_varsTable[22]].o);
			} else {
				setObjectParent(o, room->o);
			}
		}
	} else if (o->specialData[1][23] == 57) {
		o->xPos = x;
		o->yPos = y - (((int16_t)o->anim.anikeyfData[4]) << 11);
		o->zPos = z;
		x = o->xPosParent + o->xPos;
		z = o->zPosParent + o->zPos;
		if (checkCellMap(x, y)) {
			o->room = getCellMapShr19(x, z)->room;
		}
		if (_updateGlobalPosRefObject) {
			if (_updateGlobalPosRefObject->specialData[1][21] != 8) {
				GameObject *o_tmp = _currentObject;
				_currentObject = _updateGlobalPosRefObject;
				sendMessage(57, o->objKey);
				_currentObject = o_tmp;
			}
		} else {
			sendMessage(57, o->objKey);
			ret = false;
		}
		if (o != _objectsPtrTable[kObjPtrConrad] && (o->flags[1] & 0x1000) != 0) {
			GameObject *o_room = _roomsTable[o->room].o;
			if (o_room->state != 1 && o_room->state != 3) {
				setObjectParent(o, o_room);
			} else if (testObjectsRoom(o->objKey, _objectsPtrTable[kObjPtrConrad]->objKey)) {
				setObjectParent(o, _roomsTable[_varsTable[22]].o);
			} else {
				setObjectParent(o, o_room);
			}
		}
	} else {
		o->xPos += rx0;
		o->yPos -= dy << 11;
		o->zPos -= rz0;
		x = o->xPosParent + o->xPos;
		z = o->zPosParent + o->zPos;
		o->room = roomPrev;
		gotoEndScriptAnim(1);
		if (_updateGlobalPosRefObject) {
			sendMessage(57, _updateGlobalPosRefObject->objKey);
			_currentObject = _updateGlobalPosRefObject;
			int16_t objKeyTmp = _currentObjectKey;
			_currentObjectKey = _currentObject->objKey;
			sendMessage(57, o->objKey);
			_currentObject = o;
			_currentObjectKey = objKeyTmp;
		} else {
			sendMessage(57, o->objKey);
		}
		ret = false;
	}
	if (_cameraViewKey == o->objKey) {
		if (roomPrev != o->room) {
			_roomsTable[o->room].fl = 1;
			if (_snd._musicMode == 3) {
				GameObject *o_tmp = _objectsPtrTable[kObjPtrMusic];
				if ((o_tmp->flags[1] & 0x80) == 0) {
					o_tmp->room = getCellMapShr19(o_tmp->xPos, o_tmp->zPos)->room;
					o_tmp = _roomsTable[o_tmp->room].o;
				}
				if (o_tmp) {
					while ((o_tmp->state == 1) && (o_tmp != _objectsPtrTable[kObjPtrWorld])) {
						o_tmp = o_tmp->o_parent;
					}
				}
				if (o_tmp != _objectsPtrTable[kObjPtrWorld]) {
					playMusic(-1);
				}
			} else {
				playMusic(-1);
			}
			_room = o->room;
		}
		if (o->anim.anikeyfData) {
			int dx0, dy0, dz0;
			if (!_fixedViewpoint) {
				dx0 = _xPosViewpoint - _x0PosViewpoint;
				dy0 = _yPosViewpoint - _y0PosViewpoint;
				dz0 = _zPosViewpoint - _z0PosViewpoint;
				_x0PosViewpoint = _xPosViewpoint;
				_y0PosViewpoint = _yPosViewpoint;
				_z0PosViewpoint = _zPosViewpoint;
			} else {
				dx0 = dy0 = dz0 = 0;
			}
			if (flag == 0) {
				if (_cameraStep1 != 0) {
					x = _animDx;
					z = _animDz;
					int count = o->anim.aniheadData[1];
					if (count == 0) {
						count = 1;
					}
					_animDx = (int16_t)READ_LE_UINT16(o->anim.aniheadData + 10) / count;
					_animDz = (int16_t)READ_LE_UINT16(o->anim.aniheadData + 12) / count;
					x += _animDx;
					z += _animDz;
					x /= 2;
					z /= 2;
					rx0 = ((cosy * x) - (siny * z)) >> 5;
					rz0 = ((siny * x) + (cosy * z)) >> 5;
					x = o->xPosParent + o->xPos - rx0;
					y = o->yPosParent + o->yPos;
					z = o->zPosParent + o->zPos + rz0;
				}
				if (_cameraState == 0) {
					if (_cameraStep1 != 0) {
						updateCameraViewpoint(x, y, z);
					} else {
						_xPosViewpoint = o->xPosParent + o->xPos;
						_yPosViewpoint = o->yPosParent + o->yPos;
						_zPosViewpoint = o->zPosParent + o->zPos;
					}
				} else {
					_xPosViewpoint = x;
					_yPosViewpoint = y;
					_zPosViewpoint = z;
				}
				if (_cameraStep1 != 0) {
					if (!_fixedViewpoint) {
						int dx = (_xPosViewpoint - _x0PosViewpoint + dx0) / 4;
						int dy = (_yPosViewpoint - _y0PosViewpoint + dy0) / 4;
						int dz = (_zPosViewpoint - _z0PosViewpoint + dz0) / 4;
						_xPosViewpoint = _x0PosViewpoint + dx;
						_yPosViewpoint = _y0PosViewpoint + dy;
						_zPosViewpoint = _z0PosViewpoint + dz;
					}
					if (_fixedViewpoint) {
						_fixedViewpoint = false;
					}
				}
			} else {
				if (_cameraStep1 != 0) {
					rx0 = ((cosy * _animDx) - (siny * _animDz)) >> 5;
					rz0 = ((siny * _animDx) + (cosy * _animDz)) >> 5;
					if (!_fixedViewpoint) {
						if (_cameraState == 0) {
							x = o->xPosParent + o->xPos - rx0;
							y = o->yPosParent + o->yPos;
							z = o->zPosParent + o->zPos + rz0;
							updateCameraViewpoint(x, y, z);
						} else {
							_xPosViewpoint -= rx0;
							_zPosViewpoint += rz0;
						}
					} else {
						_xPosViewpoint = o->xPosParent + o->xPos - rx0;
						_yPosViewpoint = o->yPosParent + o->yPos;
						_zPosViewpoint = o->zPosParent + o->zPos + rz0;
						_fixedViewpoint = false;
					}
				} else {
					_xPosViewpoint = o->xPosParent + o->xPos;
					_yPosViewpoint = o->yPosParent + o->yPos;
					_zPosViewpoint = o->zPosParent + o->zPos;
				}
			}
		}
		if (_cameraDefaultDist && (_varsTable[9] == 2 || _varsTable[9] == 3)) {
			_xPosViewpoint = o->xPosParent + o->xPos + 6 * siny;
			_yPosViewpoint = o->yPosParent + o->yPos;
			_zPosViewpoint = o->zPosParent + o->zPos + 6 * cosy;
		}
	}
	return ret;
}

GameObject *Game::getNextObject(GameObject *o) {
	assert(o);
	if (o->o_next) {
		return o->o_next;
	} else {
		return o->o_parent->o_child;
	}
}

GameObject *Game::getPreviousObject(GameObject *o) {
	assert(o);
	GameObject *o_tmp = o->o_parent->o_child;
	if (o_tmp != o) {
		while (o_tmp->o_next != o) {
			o_tmp = o_tmp->o_next;
		}
	} else {
		while (o_tmp->o_next != 0) {
			o_tmp = o_tmp->o_next;
		}
	}
	return o_tmp;
}

void Game::setObjectParent(GameObject *o, GameObject *o_parent) {
	if (o_parent == _objectsPtrTable[kObjPtrCimetiere]) {
		o->specialData[1][8] = 0;
	}
	if (o->o_parent == o_parent) {
		return;
	}
	if (o_parent->specialData[1][21] == 128 && o->specialData[1][21] != 8) {
		if (!_objectsPtrTable[8] && o_parent == _objectsPtrTable[kObjPtrInventaire]->o_child->o_next) {
			_objectsPtrTable[8] = o;
			if (getMessage(o->objKey, 0, &_tmpMsg)) {
				o->text = (const char *)_tmpMsg.data;
			}
		}
		if (o == o->o_parent->o_child) {
			o->o_parent->o_child = o->o_next;
		} else {
			GameObject *o_tmp = getPreviousObject(o);
			o_tmp->o_next = o->o_next;
		}
		o->xPosParent = o_parent->xPosParent + o_parent->xPos;
		o->yPosParent = o_parent->yPosParent + o_parent->yPos;
		o->zPosParent = o_parent->zPosParent + o_parent->zPos;
		o->o_parent = o_parent;
		o->o_next = 0;
		o->parentChanged = true;
		GameObject *o_tmp = o_parent->o_child;
		if (!o_tmp) {
			o_parent->o_child = o;
		} else {
			while (o_tmp->o_next) {
				o_tmp = o_tmp->o_next;
			}
			o_tmp->o_next = o;
		}
	} else {
		if (_objectsPtrTable[8] && o == _objectsPtrTable[8]) {
			if (o == o->o_parent->o_child && o->o_next) {
				_objectsPtrTable[8] = 0;
				_varsTable[23] = 0;
			} else {
				if (o->o_next) {
					_objectsPtrTable[8] = o->o_next;
				} else {
					_objectsPtrTable[8] = o->o_parent->o_child;
				}
				_varsTable[23] = _objectsPtrTable[8]->objKey;
				if (getMessage(_objectsPtrTable[8]->objKey, 0, &_tmpMsg)) {
					_objectsPtrTable[8]->text = (const char *)_tmpMsg.data;
				}
			}
		}
		if (o == o->o_parent->o_child) {
			o->o_parent->o_child = o->o_next;
		} else {
			GameObject *o_tmp = getPreviousObject(o);
			o_tmp->o_next = o->o_next;
		}
		o->xPosParent = o_parent->xPosParent + o_parent->xPos;
		o->yPosParent = o_parent->yPosParent + o_parent->yPos;
		o->zPosParent = o_parent->zPosParent + o_parent->zPos;
		o->o_parent = o_parent;
		o->o_next = 0;
		o->parentChanged = true;
		GameObject *o_tmp = o_parent->o_child;
		if (!o_tmp) {
			o_parent->o_child = o;
		} else if (o_tmp->flags[1] & 0x100) {
			o->o_next = o_tmp;
			o_parent->o_child = o;
		} else {
			while (o_tmp->o_next && (o_tmp->o_next->flags[1] & 0x100) == 0) {
				o_tmp = o_tmp->o_next;
			}
			assert(o != o_tmp);
			o->o_next = o_tmp->o_next;
			o_tmp->o_next = o;
		}
	}
}

void Game::fixRoomDataHelper(int x, int z, uint8_t room) {
	CellMap *cell = getCellMap(x, z);
	if (!cell->fixed) {
		cell->fixed = true;
		if (cell->type != 0) {
			return;
		}
		if (cell->room2 == room) {
			debug(kDebug_GAME, "Game::fixRoomDataHelper room %d room2 %d\n", cell->room, cell->room2);
			SWAP(cell->room, cell->room2);
		}
	}
	static const int dxCell[] = { -1,  0,  1, -1,  1, -1,  0,  1 };
	static const int dzCell[] = {  1,  1,  1,  0,  0, -1, -1, -1 };
	for (int i = 0; i < 8; ++i) {
		int cx = x + dxCell[i];
		int cz = z + dzCell[i];
		assert(cx >= 0 && cx < 64 && cz >= 0 && cz < 64);
		if (!_sceneCellMap[cx][cz].fixed) {
			fixRoomDataHelper(cx, cz, room);
		}
	}
}

void Game::fixRoomData() {
	for (int x = 0; x < kMapSizeX; ++x) {
		for (int z = 0; z < kMapSizeZ; ++z) {
			CellMap *cell = &_sceneCellMap[x][z];
			if (cell->type == -2) {
				cell->type = 0;
			}
			cell->isDoor = (cell->type >= 4 && cell->type <= 7) || (cell->type >= 16 && cell->type <= 19);
		}
	}
	for (int x = 0; x < kMapSizeX - 1; ++x) {
		for (int z = 0; z < kMapSizeZ - 1; ++z) {
			CellMap *cell = &_sceneCellMap[x][z];
			if (!cell->fixed && cell->type == 0 && cell->room2 == 0) {
				fixRoomDataHelper(x, z, cell->room);
			}
		}
	}
}

void Game::updateObjects() {
	updateParticles();
	drawParticles();
}

void Game::doTick() {
	const int currentRoom = _room;
	updateSceneAnimations();
	updateSceneTextures();
	if (_mainLoopCurrentMode == 0) {
		if (_viewportSize > 0) {
			_viewportSize -= 2;
	                initViewport();
		} else {
			_viewportSize = 0;
			_mainLoopCurrentMode = 1;
		}
	}
	if (_mainLoopCurrentMode == 1) {
		switch (_conradHit) {
		case 2:
			_render->setOverlayBlendColor(255, 0, 0);
			_conradHit = 1;
			break;
		case 1:
			_conradHit = 0;
			break;
		}
	}
	updatePlayerObject();
	redrawScene();
	if (_changedObjectsCount != 0) {
		updateChangedObjects();
	}
	addObjectsToScene();
	updateObjects();
	++_ticks;
	if ((_cheats & kCheatLifeCounter) != 0) {
		_objectsPtrTable[kObjPtrConrad]->specialData[1][18] = _varsTable[kVarConradLife];
	}
	runObject(_objectsPtrTable[kObjPtrWorld]->o_child);
if (_mainLoopCurrentMode == 1) {
	GameObject *o_ply = getObjectByKey(_varsTable[kVarPlayerObject]);
	CellMap *cell = getCellMapShr19(o_ply->xPosParent + o_ply->xPos, o_ply->zPosParent + o_ply->zPos);
	if (cell->room != 0 && cell->room2 == 0) {
		GameObject *o_room = _roomsTable[cell->room].o;
		assert(o_room);
		if (o_room->state != 1) {
			while (o_room != _objectsPtrTable[kObjPtrWorld]) {
				o_room->state = 1;
				o_room = o_room->o_parent;
			}
		}
	}
}
	clearObjectsDrawList();
if (_mainLoopCurrentMode == 1) {
	if (_objectsPtrTable[10]) {
		const int scannerLife = _objectsPtrTable[10]->specialData[1][18];
		const int conradLife = _objectsPtrTable[kObjPtrConrad]->specialData[1][18];
		if (scannerLife || (conradLife >= scannerLife && (conradLife >= scannerLife * 36 || (_ticks % (scannerLife / 2) > scannerLife / 4)))) {
			if (_objectsPtrTable[10]->specialData[1][21] == 64) {
				warning("Game::doTick() Unimplemented scanner");
			}
		}
		if (_object10Counter <= 0) {
			_object10Counter = 10;
			_objectsPtrTable[kObjPtrConrad]->specialData[1][18] -= _objectsPtrTable[10]->specialData[1][18];
			if (_objectsPtrTable[kObjPtrConrad]->specialData[1][18] <= 0) {
				_objectsPtrTable[kObjPtrConrad]->specialData[1][18] = 1;
				while ((_objectsPtrTable[10]->specialData[1][21] != 2) && (_objectsPtrTable[10]->specialData[1][22] != 0xFFFF)) {
					GameObject *o_tmp = _objectsPtrTable[10];
					while (o_tmp->o_next) {
						o_tmp = o_tmp->o_next;
					}
					o_tmp->o_next = _objectsPtrTable[10];
					_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_next->o_child = _objectsPtrTable[10]->o_next;
					_objectsPtrTable[10]->o_next = 0;
					_objectsPtrTable[10] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_next->o_child;
				}
				if (_objectsPtrTable[10]) {
					_varsTable[24] = _objectsPtrTable[10]->objKey;
					if (getMessage(_objectsPtrTable[10]->objKey, 0, &_tmpMsg)) {
						_objectsPtrTable[10]->text = (const char *)_tmpMsg.data;
					}
				}
			}
		} else {
			--_object10Counter;
		}
	}
	if ((_objectsPtrTable[kObjPtrConrad]->specialData[1][20] & 15) == 5) {
		_objectsPtrTable[kObjPtrConrad]->specialData[1][18] -= 2;
		if (_objectsPtrTable[kObjPtrConrad]->specialData[1][18] < 2) {
			_objectsPtrTable[kObjPtrConrad]->specialData[1][18] = 2;
			while (_objectsPtrTable[7]->specialData[1][22] != 0) {
				GameObject *o_tmp = _objectsPtrTable[7];
				while (o_tmp->o_next) {
					o_tmp = o_tmp->o_next;
				}
				o_tmp->o_next = _objectsPtrTable[7];
				_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child = _objectsPtrTable[7]->o_next;
				_objectsPtrTable[7]->o_next = 0;
				_objectsPtrTable[7] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child;
			}
			if (_objectsPtrTable[7]) {
				_varsTable[25] = _objectsPtrTable[7]->objKey;
				if (getMessage(_objectsPtrTable[7]->objKey, 0, &_tmpMsg)) {
					_objectsPtrTable[7]->text = (const char *)_tmpMsg.data;
				}
			}
			setObjectData(_objectsPtrTable[kObjPtrConrad], 20, 0);
		}
	}
	if (_objectsPtrTable[7]->specialData[1][22] == 2) {
		_objectsPtrTable[kObjPtrConrad]->specialData[1][18] -= 1;
		if (_objectsPtrTable[kObjPtrConrad]->specialData[1][18] < 1) {
			_objectsPtrTable[kObjPtrConrad]->specialData[1][18] = 1;
			while (_objectsPtrTable[7]->specialData[1][22] != 0) {
				GameObject *o_tmp = _objectsPtrTable[7];
				while (o_tmp->o_next) {
					o_tmp = o_tmp->o_next;
				}
				o_tmp->o_next = _objectsPtrTable[7];
				_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child = _objectsPtrTable[7]->o_next;
				_objectsPtrTable[7]->o_next = 0;
				_objectsPtrTable[7] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child;
			}
			if (_objectsPtrTable[7]) {
				_varsTable[25] = _objectsPtrTable[7]->objKey;
				if (getMessage(_objectsPtrTable[7]->objKey, 0, &_tmpMsg)) {
					_objectsPtrTable[7]->text = (const char *)_tmpMsg.data;
				}
			}
		}
	}
	_render->setupProjection2d();
	drawInfoPanel();
#ifdef F2B_DEBUG
	if (1) {
		int y = 8;
		GameObject *o = _objectsPtrTable[kObjPtrConrad];
		char buf[64];
		snprintf(buf, sizeof(buf), "conrad.pos %d %d %d pitch %d", o->xPos >> kPosShift, o->zPos >> kPosShift, o->yPos >> kPosShift, o->pitch);
		drawString(8, y, buf, kFontNormale, 0);
		y += 8;
		snprintf(buf, sizeof(buf), "camera.pos %d %d %d pitch %d", _xPosObserver >> kPosShift, _zPosObserver >> kPosShift, _yPosObserver >> kPosShift, _yRotObserver);
		drawString(8, y, buf, kFontNormale, 0);
		y += 8;
	}
#endif
	if (_cut._numToPlayCounter >= 0) {
		if (_cut._numToPlayCounter == 0) {
			if (_cut._numToPlay >= 0) {
//				playCutscene(_cut._numToPlay);
//				_cut._numToPlay = -1;
			}
		}
		--_cut._numToPlayCounter;
	}
	if (_changeLevel) {
//		setPaletteColor(1, 255, 255, 255);
		if (getMessage(_objectsPtrTable[kObjPtrFadeToBlack]->objKey, 1, &_tmpMsg)) {
			memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));
			int w, h;
			getStringRect((const char *)_tmpMsg.data, kFontNameCineTypo, &w, &h);
			drawString((kScreenWidth - w) / 2, kScreenHeight / 2, (const char *)_tmpMsg.data, kFontNameCineTypo, 0);
		}
	} else if (!_endGame) {
		updateScreen();
	}
	if (currentRoom != _room) {
		changeRoom(currentRoom);
	}
} else if (_mainLoopCurrentMode == 0) {
	_render->setupProjection2d();
	updateScreen();
}
	if (_collidingObjectsCount != 0) {
		updateCollidingObjects();
	}
}

void Game::initSprite(int type, int16_t key, SpriteImage *spr) {
	assert(type == kResType_SPR);
	uint8_t *p = _res.getData(type, key, "BTMDESC");
	spr->w = READ_LE_UINT16(p);
	spr->h = READ_LE_UINT16(p + 2);
	spr->data = _res.getData(type, key, "SPRDATA");
	spr->key = key;
}

uint8_t *Game::initMesh(int resType, int16_t key, uint8_t **verticesData, uint8_t **polygonsData, GameObject *o, uint8_t **poly3dData, int *env) {
	int num, index;
	int16_t envKey, polyKey;
	uint8_t *p_form3d, *p_poly3d, *p_envani, *p;

	assert(resType == kResType_F3D);
	p_form3d = _res.getData(resType, key, "FORM3D");
	*verticesData = _res.getData(resType, key, "F3DDATA");

	polyKey = READ_LE_UINT16(p_form3d + 16);
	if (!env || *env == 0) {
		envKey = polyKey;
	} else {
get_envani:
		num = *env & 255;
		index = (*env >> 16) & 255;
		p_envani = _res.getEnvAni(polyKey, num);
		if (!p_envani) {
			envKey = polyKey;
			*env = 0;
		} else {
			envKey = READ_LE_UINT16(p_envani + 2 + index * 2);
			if (envKey == 0) {
				if (o == _objectsPtrTable[kObjPtrConrad] && _conradEnvAni != 0) {
					num = _conradEnvAni & 255;
					index = (_conradEnvAni >> 16) & 255;
					*env = (index << 16) | num;
					goto get_envani;
				}
				num = 0;
				index = 0;
				envKey = READ_LE_UINT16(p_envani + 2 + index * 2);
			} else if (envKey < 0) {
				index += envKey;
				if (index < 0 || index >= 16) {
					index = 0;
				}
				envKey = READ_LE_UINT16(p_envani + 2 + index * 2);
			} else {
				++index;
				if (index < 0 || index >= 16) {
					index = 0;
				}
			}
			*env = (index << 16) | num;
		}
	}
	*polygonsData = _res.getData(kResType_P3D, envKey, "P3DDATA");
	p_poly3d = _res.getData(kResType_P3D, envKey, "POLY3D");
	if (env && *env != 0) {
		p = _res.getData(kResType_P3D, polyKey, "POLY3D");
		if (READ_LE_UINT32(p) != READ_LE_UINT32(p_poly3d)) {
			if (o->specialData[0][20]) {
				*env = o->specialData[0][20];
				goto get_envani;
			}
			*env = 0;
			p_poly3d = p;
		}
	}
	*poly3dData = p_poly3d;
	return p_form3d;
}

bool Game::addSceneObjectToList(int xPos, int yPos, int zPos, GameObject *o) {
	assert(_sceneObjectsCount < kSceneObjectsTableSize);
	SceneObject *so = &_sceneObjectsTable[_sceneObjectsCount];
	int16_t key = _res.getChild(kResType_ANI, o->anim.currentAnimKey);
	if (key == 0) {
		if (o->objKey == _cameraViewKey) {
			_yRotViewpoint = o->pitch;
			_cameraViewObj = _sceneObjectsCount;
		}
		return false;
	}
	uint8_t *p_anifram = _res.getData(kResType_ANI, key, "ANIFRAM");
	assert(p_anifram != 0);
	o->anim.aniframData = p_anifram;
	debug(kDebug_GAME, "Game::addSceneObjectToList o %p key %d tree %d", o, key, p_anifram[2]);
	if (p_anifram[2] == 9) {
		uint8_t *p_poly3d;
		uint8_t *p_form3d = initMesh(kResType_F3D, READ_LE_UINT16(p_anifram), &so->verticesData, &so->polygonsData, o, &p_poly3d, &o->specialData[1][20]);
		assert(p_form3d != 0);
		so->verticesCount = READ_LE_UINT16(p_form3d + 18);
		assert(so->verticesCount != 0);
		so->xmin = READ_LE_UINT32(p_form3d);
		so->zmin = READ_LE_UINT32(p_form3d + 4);
		so->xmax = READ_LE_UINT32(p_form3d + 8);
		so->zmax = READ_LE_UINT32(p_form3d + 12);
		so->pitch = (o->pitch + (p_anifram[3] << 4)) & 1023;
		so->x = xPos;
		so->y = yPos;
		so->z = zPos;
		so->zBuf = 0;
	} else {
		assert(p_anifram[2] == 1);
		initSprite(kResType_SPR, READ_LE_UINT16(p_anifram), &so->spr);
		so->verticesCount = 0;
		so->xmin = so->zmin = 0;
		so->xmax = so->zmax = 0;
		GameObject *o_parent = o->o_parent;
		const uint8_t *p_anikeyf = o_parent->anim.anikeyfData;
		if (o_parent && p_anikeyf && o == _objectsPtrTable[kObjPtrTarget]) {
			const int xo = (((int8_t)p_anikeyf[16]) * 8 + (int16_t)READ_LE_UINT16(p_anikeyf + 2)) << 10;
			const int zo = (((int8_t)p_anikeyf[17]) * 8 + (int16_t)READ_LE_UINT16(p_anikeyf + 6)) << 10;
			int cosy =  g_cos[o_parent->pitch & 1023];
			int siny = -g_sin[o_parent->pitch & 1023];
			const int xr = fixedMul(cosy, xo, kPosShift) - fixedMul(siny, zo, kPosShift);
			so->x = o_parent->xPosParent + o->xPos;
			so->x += xr;
			so->y = o_parent->yPosParent + o->yPos;
			so->y += ((int16_t)READ_LE_UINT16(p_anikeyf + 14)) << 11;
			const int zr = fixedMul(siny, xo, kPosShift) + fixedMul(cosy, zo, kPosShift);
			so->z = o_parent->zPosParent + o->zPos;
			so->z += zr;
		} else {
			so->x = xPos;
			so->y = yPos;
			so->z = zPos;
		}
		so->zBuf = -16;
	}
	so->o = o;
	if (o->inSceneList) {
		if (o->objKey == _cameraViewKey) {
			_yRotViewpoint = o->pitch;
			_cameraViewObj = _sceneObjectsCount;
		}
		++_sceneObjectsCount;
		drawSceneObject(so);
		return true;
	} else {
#if 1 // TEMP
		if (o->objKey == _cameraViewKey) {
			warning("Game::addSceneObjectToList() cameraObject not visible");
			_yRotViewpoint = o->pitch;
		}
#endif
	}
	return false;
}

void Game::clearObjectsDrawList() {
	for (int i = 0; i < _objectsDrawCount; ++i) {
		GameObject *o = _objectsDrawList[i];
		if (o->flags[1] & 0x200) {
			o->state = 0;
		}
	}
	_objectsDrawCount = 0;
}

void Game::addObjectsToScene() {
	_sceneObjectsCount = 0;
	bool cameraInList = false;
	for (int i = 0; i < _objectsDrawCount; ++i) {
		GameObject *o = _objectsDrawList[i];
		if (!cameraInList && o->objKey == _cameraViewKey) {
			cameraInList = true;
		}
		int xPos, yPos, zPos;
		if (o->flags[1] & 0x100) {
			xPos = o->xPos;
			assert(o->anim.anikeyfData);
			int16_t dy = READ_LE_UINT16(o->anim.anikeyfData + 4);
			yPos = o->yPos - (dy << 11);
			zPos = o->zPos;
		} else {
			xPos = o->xPosWorld;
			yPos = o->yPosWorld;
			zPos = o->zPosWorld;
		}
		addSceneObjectToList(xPos, yPos, zPos, o);
//		if (strcmp(o->name, "conrad") == 0) printf("conrad.pos %d,%d %d\n", o->xPos >> 19, o->zPos >> 19, o->yPos >> 19);
//		if (strcmp(o->name, "eclair_energie_S12_A") == 0) printf("eclair_energie_S12_A.pos %d,%d,%d flags 0x%X\n", xPos, yPos, zPos, (int)o->flags[1]);
//		if (strncmp(o->name, "tir", 3) == 0) printf("tir.pos %d,%d %d\n", o->xPos >> 19, o->zPos >> 19, o->yPos >> 19);
	}
	if (!cameraInList) {
		GameObject *o = getObjectByKey(_cameraViewKey);
		addSceneObjectToList(o->xPosWorld, o->yPosWorld, o->zPosWorld, o);
	}
}

void Game::clearKeyboardInput() {
	_inputDirKeyPressed[0] = 0;
	_inputDirKeyPressed[1] = 0;
	_inputButtonKey[0] = 0;
	_inputButtonKey[1] = 0;
	if (_inputsCount != 0) {
		const int inp = getCurrentInput();
		_inputsTable[inp].keymask = 0;
	}
}

int Game::getCurrentInput() {
	int16_t objKey = _varsTable[kVarPlayerObject];
	GameObject *o = getObjectByKey(objKey);
	if (!o) {
		o = _objectsPtrTable[kObjPtrConrad];
	}
	const int index = o->customData[11];
	return index;
}

void Game::clearMessage(ResMessageDescription *desc) {
	memset(desc, 0, sizeof(ResMessageDescription));
	desc->font = 0;
}

bool Game::getMessage(int16_t key, uint32_t value, ResMessageDescription *desc) {
	clearMessage(desc);
	const int offset = _res.getOffsetForObjectKey(key);
	return offset != -1 && _res.getMessageDescription(desc, value, offset);
}

#ifdef BUFFER_FLATPOLYGONS
float floorY=0;
#endif

void Game::drawSceneObjectMesh(const uint8_t *polygonsData, const uint8_t *verticesData, int verticesCount) {
	if (polygonsData[0] & 0x80) {
		const int shadowPolySize = -(int8_t)polygonsData[0];
//		drawSceneObjectMeshShadow(&polygonsData[1], verticesData, verticesCount);
		polygonsData += shadowPolySize;
	}
	int count = *polygonsData++;
	int color = READ_LE_UINT16(polygonsData); polygonsData += 2;
	while (count != 0) {
		Vertex polygonPoints[16];
		if (count & 0x40) {
			count &= 15;
			for (int i = 0; i <= count; ++i) {
				int index = *polygonsData++;
				if (index >= verticesCount) {
					warning("Game::drawSceneObjectMesh() invalid index %d in vertex buffer (size %d)", index, verticesCount);
					return;
				}
				assert(index < verticesCount);
				polygonPoints[i] = READ_VERTEX32(verticesData + index * 4);
			}
		} else {
			count &= 15;
			for (int i = 0; i <= count; ++i) {
				int index = READ_LE_UINT16(polygonsData); polygonsData += 2;
				assert(index < verticesCount);
				polygonPoints[i] = READ_VERTEX32(verticesData + index * 4);
			}
		}
		const int fill = (color >> 8) & 31;
		if (fill == 8 || fill == 11 || fill == 12) {
			const int texture = color & 255;
			const int primitive = (color >> 13) & 7;
			if (!_sceneTextureImagesBuffer[texture].data) { // level3
				warning("Game::drawSceneObjectMesh() no sprite for texture %d", texture);
			} else {
				SpriteImage *spr = &_sceneTextureImagesBuffer[texture];
				const uint8_t *texData = _spriteCache.getData(spr->key, spr->data);
				_render->drawPolygonTexture(polygonPoints, count + 1, primitive, texData, spr->w, spr->h, spr->key);
			}
			// 11 : transparent, pixel != 0 pixel + scolor
			//  8 : palette remap, pixel + scolor
			// 12 : pixel + 0
		} else if (fill != 10) {
			const int primitive = color >> 8;
			switch (primitive) {
			case 0:
			case 1:
				color &= 255;
				break;
			case 2:
				color = kFlatColorShadow;
				break;
			case 3:
				color = kFlatColorLight;
				break;
			case 4:
				color = kFlatColorGreen;
				break;
			case 5:
				color = kFlatColorRed;
				break;
			case 6:
				color = kFlatColorYellow;
				break;
			case 7:
				color = kFlatColorBlue;
				break;
			case 8:
				color = kFlatColorShadow;
				break;
			case 9:
				color = kFlatColorLight9;
				break;
			default:
				warning("Game::drawSceneObjectMesh() unhandled primitive %d", primitive);
				color = -1;
				break;
			}
			if (color >= 0) {
				_render->drawPolygonFlat(polygonPoints, count + 1, color);
			}
		}
		count = *polygonsData++;
		color = READ_LE_UINT16(polygonsData); polygonsData += 2;
	}
#ifdef BUFFER_FLATPOLYGONS
	_render->flushPolygonFlat(floorY*2, false);
#endif
}

void Game::drawSceneObject(SceneObject *so) {
	if (so->verticesCount != 0) {
#ifdef BUFFER_FLATPOLYGONS
		floorY = so->y / (float)(1 << kPosShift);
#endif
		_render->beginObjectDraw(so->x, so->y, so->z, so->o->pitch, kPosShift);
		assert(so->polygonsData != 0 && so->verticesData != 0);
		drawSceneObjectMesh(so->polygonsData, so->verticesData, so->verticesCount);
		_render->endObjectDraw();
	} else {
		SpriteImage *spr = &so->spr;
		const uint8_t *texData = _spriteCache.getData(spr->key, spr->data);
		_render->beginObjectDraw(so->x, (kGroundY << kPosShift) + so->y, so->z, _yInvRotObserver, kPosShift);
		const int scale = (so->o->flags[1] & 0x20000) != 0 ? 2 : 1;
		const int x0 = -scale * spr->w / 2;
		const int y0 = -scale * spr->h / 2;
		const int x1 = x0 + spr->w;
		const int y1 = y0 + spr->h;
		Vertex v[4];
		v[0].x = x0; v[0].y = y0; v[0].z = 0;
		v[1].x = x1; v[1].y = y0; v[1].z = 0;
		v[2].x = x1; v[2].y = y1; v[2].z = 0;
		v[3].x = x0; v[3].y = y1; v[3].z = 0;
		_render->drawPolygonTexture(v, 4, 0, texData, spr->w, spr->h, spr->key);
		_render->endObjectDraw();
	}
}

void Game::redrawScene() {
	_fixedViewpoint = false;
	updateObserverSinCos();
	updateSceneCameraPos();
	updateObserverSinCos();
	++_rayCastCounter;
	redrawSceneGroundWalls();
}

#ifdef BUFFER_TEXTPOLYGONS
void Game::cached_drawWall(const Vertex *vertices, int verticesCount, int texture) {
	texture &= 4095;
	if (texture >= 0  && texture < 512) {
		_sceneAnimationsTable[texture].type |= 0x10;
		if (texture != 0 && texture != 1) {
			SpriteImage *spr = &_sceneAnimationsTextureTable[texture];
			if (spr->data) {
				const uint8_t *texData = _spriteCache.getData(spr->key, spr->data);
				_render->cached_drawPolygonTexture(vertices, verticesCount, 0, texData, spr->w, spr->h, spr->key);
			}
		}
	}
}
#endif

void Game::drawWall(const Vertex *vertices, int verticesCount, int texture) {
	texture &= 4095;
	if (texture >= 0  && texture < 512) {
		_sceneAnimationsTable[texture].type |= 0x10;
		if (texture != 0 && texture != 1) {
			SpriteImage *spr = &_sceneAnimationsTextureTable[texture];
			if (spr->data) {
				const uint8_t *texData = _spriteCache.getData(spr->key, spr->data);
				_render->drawPolygonTexture(vertices, verticesCount, 0, texData, spr->w, spr->h, spr->key);
			}
		}
	}
}

static void initVerticesW(Vertex *quad, int x, int z, int dx, int dz) {
	quad[0].x = x * 16 + dx;
	quad[0].y = 0;
	quad[0].z = (z + 1) * 16 + dz;

	quad[1].x = x * 16 + dx;
	quad[1].y = kGroundY;
	quad[1].z = (z + 1) * 16 + dz;

	quad[2].x = x * 16 + dx;
	quad[2].y = kGroundY;
	quad[2].z = z * 16 + dz;

	quad[3].x = x * 16 + dx;
	quad[3].y = 0;
	quad[3].z = z * 16 + dz;
}

static void initVerticesS(Vertex *quad, int x, int z, int dx, int dz) {
	quad[0].x = x * 16 + dx;
	quad[0].y = 0;
	quad[0].z = z * 16 + dz;

	quad[1].x = x * 16 + dx;
	quad[1].y = kGroundY;
	quad[1].z = z * 16 + dz;

	quad[2].x = (x + 1) * 16 + dx;
	quad[2].y = kGroundY;
	quad[2].z = z * 16 + dz;

	quad[3].x = (x + 1) * 16 + dx;
	quad[3].y = 0;
	quad[3].z = z * 16 + dz;
}

static void initVerticesE(Vertex *quad, int x, int z, int dx, int dz) {
	quad[0].x = (x + 1) * 16 + dx;
	quad[0].y = 0;
	quad[0].z = z * 16 + dz;

	quad[1].x = (x + 1) * 16 + dx;
	quad[1].y = kGroundY;
	quad[1].z = z * 16 + dz;

	quad[2].x = (x + 1) * 16 + dx;
	quad[2].y = kGroundY;
	quad[2].z = (z + 1) * 16 + dz;

	quad[3].x = (x + 1) * 16 + dx;
	quad[3].y = 0;
	quad[3].z = (z + 1) * 16 + dz;
}

static void initVerticesN(Vertex *quad, int x, int z, int dx, int dz) {
	quad[0].x = (x + 1) * 16 + dx;
	quad[0].y = 0;
	quad[0].z = (z + 1) * 16 + dz;

	quad[1].x = (x + 1) * 16 + dx;
	quad[1].y = kGroundY;
	quad[1].z = (z + 1) * 16 + dz;

	quad[2].x = x * 16 + dx;
	quad[2].y = kGroundY;
	quad[2].z = (z + 1) * 16 + dz;

	quad[3].x = x * 16 + dx;
	quad[3].y = 0;
	quad[3].z = (z + 1) * 16 + dz;
}

static void initVerticesGround(Vertex *quad, int x, int z) {
	quad[0].x = x * 16;
	quad[0].y = kGroundY;
	quad[0].z = z * 16;

	quad[1].x = (x + 1) * 16;
	quad[1].y = kGroundY;
	quad[1].z = z * 16;

	quad[2].x = (x + 1) * 16;
	quad[2].y = kGroundY;
	quad[2].z = (z + 1) * 16;

	quad[3].x = x * 16;
	quad[3].y = kGroundY;
	quad[3].z = (z + 1) * 16;
}

bool Game::redrawSceneGridCell(int x, int z, CellMap *cell) {
	Vertex quad[4];
	initVerticesGround(quad, x, z);
#ifdef BUFFER_TEXTPOLYGONS
	int visible = _render->isQuadInFrustrum(quad, 4);
#else
	if (!_render->isQuadInFrustrum(quad, 4)) {
		return false;
	}
#endif
	if (cell->type != 32) {
		const int index = _sceneGroundMap[x][z];
		if (index >= 0 && index < 512) {
			_sceneAnimationsTable[index].type |= 0x10;
			if (index != 0 && index != 1) {
				SpriteImage *spr = &_sceneAnimationsTextureTable[index];
				if (spr->data) {
					const uint8_t *texData = _spriteCache.getData(spr->key, spr->data);
#ifdef BUFFER_TEXTPOLYGONS
					_render->cached_drawPolygonTexture(quad, 4, 9, texData, spr->w, spr->h, spr->key);
#else
					_render->drawPolygonTexture(quad, 4, 9, texData, spr->w, spr->h, spr->key);
#endif
				}
			}
		}
	}



#ifdef BUFFER_TEXTPOLYGONS
	if (cell->type == 1) {
		initVerticesW(quad, x, z, 0, 0);
		cached_drawWall(quad, 4, cell->west);
		initVerticesS(quad, x, z, 0, 0);
		cached_drawWall(quad, 4, cell->south);
		initVerticesE(quad, x, z, 0, 0);
		cached_drawWall(quad, 4, cell->east);
		initVerticesN(quad, x, z, 0, 0);
		cached_drawWall(quad, 4, cell->north);
	}
	else if ((cell->type > 0) && visible) {
#else
	if (cell->type > 0) {
#endif
		int dx = 0, dz = 0;
		switch (cell->type) {
#ifndef BUFFER_TEXTPOLYGONS
		case 1:
			initVerticesW(quad, x, z, 0, 0);
			drawWall(quad, 4, cell->west);
			initVerticesS(quad, x, z, 0, 0);
			drawWall(quad, 4, cell->south);
			initVerticesE(quad, x, z, 0, 0);
			drawWall(quad, 4, cell->east);
			initVerticesN(quad, x, z, 0, 0);
			drawWall(quad, 4, cell->north);
			break;
#endif
		case 3:
			initVerticesS(quad, x, z, 0, 0);
			drawWall(quad, 4, cell->texture[1]);
			initVerticesS(quad, x, z, 0, -kWallThick);
			drawWall(quad, 4, cell->texture[0]);
			break;
		case 4:
		case 16:
			dz = -(63 - cell->data[1]) / 4;
			initVerticesE(quad, x, z, 0, dz);
			drawWall(quad, 4, cell->texture[1]);
			initVerticesE(quad, x, z, -kWallThick, dz);
			drawWall(quad, 4, cell->texture[0]);
			break;
		case 5:
		case 17:
			dz = (63 - cell->data[1]) / 4;
			initVerticesE(quad, x, z, 0, dz);
			drawWall(quad, 4, cell->texture[1]);
			initVerticesE(quad, x, z, -kWallThick, dz);
			drawWall(quad, 4, cell->texture[0]);
			break;
		case 6:
		case 18:
			dx = -(63 - cell->data[1]) / 4;
			initVerticesN(quad, x, z, dx, -(16 - kWallThick) / 2);
			drawWall(quad, 4, cell->texture[0]);
			initVerticesS(quad, x, z, dx,  (16 - kWallThick) / 2);
			drawWall(quad, 4, cell->texture[1]);
			break;
		case 7:
		case 19:
			dx = (63 - cell->data[1]) / 4;
			initVerticesN(quad, x, z, dx, -(16 - kWallThick) / 2);
			drawWall(quad, 4, cell->texture[0]);
			initVerticesS(quad, x, z, dx,  (16 - kWallThick) / 2);
			drawWall(quad, 4, cell->texture[1]);
			break;
		case 10:
                        initVerticesE(quad, x, z,  kWallThick/2, 0);
                        drawWall(quad, 4, cell->texture[1]);
                        initVerticesE(quad, x, z, -kWallThick/2, 0);
                        drawWall(quad, 4, cell->texture[0]);
			break;
		case 11:
                        initVerticesN(quad, x, z, 0, -(16 - kWallThick) / 2);
                        drawWall(quad, 4, cell->texture[0]);
                        initVerticesS(quad, x, z, 0,  (16 - kWallThick) / 2);
                        drawWall(quad, 4, cell->texture[1]);
			break;
		case 20:
			break;
		case 32:
			break;
		default:
			warning("Game::redrawScene() unhandled type %d (room %d x %d z %d)", cell->type, cell->room, x, z);
			break;
		}
	}
#ifndef BUFFER_TEXTPOLYGONS
	return true;
#else
	return visible;
#endif
}

void Game::redrawSceneGroundWalls() {
//	rayCast(_xPosObserver << 1, _zPosObserver << 1);
	_render->setupProjection();
	for (int x = 0; x < kMapSizeX; ++x) {
		for (int z = 0; z < kMapSizeZ; ++z) {
			CellMap *cell = &_sceneCellMap[x][z];
			const bool visible = redrawSceneGridCell(x, z, cell);
			if (!visible) {
				Vertex quad[8];
				initVerticesGround(&quad[0], x, z);
				initVerticesGround(&quad[4], x, z);
				for (int i = 0; i < 4; ++i) {
					quad[i].y = 0;
				}
				if (!_render->isBoxInFrustrum(quad, 8)) {
					continue;
				}
			}
			switch (cell->type) {
			case 0:
			case -2:
			case -3:
case 32: // fixes objects on hole (level 4)
				addObjectToDrawList(cell);
				break;
			}
		}
	}
#ifdef BUFFER_TEXTPOLYGONS
	_render->renderQuads();
#endif
}

bool Game::findRoom(const CollisionSlot *colSlot, int room1, int room2) {
	for (; colSlot; colSlot = colSlot->next) {
		const CellMap *cell = colSlot->cell;
		if (room1 != 0 && (room1 == cell->room || room1 == cell->room2)) {
			_varsTable[22] = room1;
			return true;
		}
		if (room2 != 0 && (room2 == cell->room || room2 == cell->room2)) {
			_varsTable[22] = room2;
			return true;
		}
	}
	return false;
}

bool Game::testObjectsRoom(int16_t obj1Key, int16_t obj2Key) {
	GameObject *o1 = (obj1Key == 0) ? _currentObject : getObjectByKey(obj1Key);
	if (!o1 || !o1->colSlot) {
		return false;
	}
	GameObject *o2 = getObjectByKey(obj2Key);
	if (!o2 || !o2->colSlot) {
		return false;
	}
	const CellMap *o1_cell = o1->colSlot->cell;
	if (findRoom(o2->colSlot, o1_cell->room, o1_cell->room2)) {
		return true;
	}
	const CellMap *o2_cell = o2->colSlot->cell;
	if (findRoom(o1->colSlot, o2_cell->room, o2_cell->room2)) {
		return true;
	}
	return false;
}

void Game::readInputEvents() {
	GameObject *o = _objectsPtrTable[kObjPtrGun];
	if (o->specialData[1][22] != 0x1000 && o->customData[1] == 0 && o->customData[0] == 0) {
		GameObject *o_tmp = o;
		while (o_tmp->o_next) {
			o_tmp = o_tmp->o_next;
		}
		o_tmp->o_next = o;
		_objectsPtrTable[kObjPtrInventaire]->o_child->o_child = o->o_next;
		o->o_next = 0;
		_objectsPtrTable[kObjPtrTargetCommand]->specialData[1][16] = 1;
		_objectsPtrTable[kObjPtrGun] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_child;
		if (_objectsPtrTable[kObjPtrGun]) {
			_varsTable[15] = _objectsPtrTable[kObjPtrGun]->objKey;
			if (getMessage(_objectsPtrTable[kObjPtrGun]->objKey, 0, &_tmpMsg)) {
				_objectsPtrTable[kObjPtrGun]->text = (const char *)_tmpMsg.data;
			}
		}
		setObjectParent(o, _objectsPtrTable[kObjPtrCimetiere]);
	}
	o = _objectsPtrTable[9];
	if (o && o->customData[0] == 0) {
		if (o->o_next) {
			GameObject *o_tmp = o;
			while (o_tmp->o_next) {
				o_tmp = o_tmp->o_next;
			}
			o_tmp->o_next = o;
			_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_child = o->o_next;
			o->o_next = 0;
			_objectsPtrTable[9] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_child;
			if (_objectsPtrTable[9]) {
				if (getMessage(_objectsPtrTable[9]->objKey, 0, &_tmpMsg)) {
					_objectsPtrTable[9]->text = (const char *)_tmpMsg.data;
				}
			}
			setObjectParent(o, _objectsPtrTable[kObjPtrCimetiere]);
		} else {
			GameObject *o_prev = _objectsPtrTable[9];
			setObjectParent(o_prev, _objectsPtrTable[kObjPtrCimetiere]);
			_objectsPtrTable[9] = _objectsPtrTable[1]->o_child->o_next->o_next->o_child = 0;
		}
	}
	if (_res._levelDescriptionsTable[_level].inventory) {
		const int num = _objectsPtrTable[kObjPtrConrad]->specialData[1][20];
		if (num != 8 && num != 9) {
			if (inp.numKeys[4]) {
				inp.numKeys[4] = false;
				if (_objectsPtrTable[kObjPtrGun] && _objectsPtrTable[kObjPtrGun]->o_next) {
					GameObject *tmpObj = _objectsPtrTable[kObjPtrGun];
					while (tmpObj->o_next) {
						tmpObj = tmpObj->o_next;
					}
					tmpObj->o_next = _objectsPtrTable[kObjPtrGun];
					_objectsPtrTable[kObjPtrInventaire]->o_child->o_child = _objectsPtrTable[kObjPtrGun]->o_next;
					_objectsPtrTable[kObjPtrGun]->o_next = 0;
					_objectsPtrTable[kObjPtrTargetCommand]->specialData[1][16] = 1;
					_objectsPtrTable[kObjPtrGun] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_child;
					if (_objectsPtrTable[kObjPtrGun]) {
						_varsTable[15] = _objectsPtrTable[kObjPtrGun]->objKey;
						if (getMessage(_objectsPtrTable[kObjPtrGun]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[kObjPtrGun]->text = (const char *)_tmpMsg.data;
						}
					}
				}
			}
			if (inp.numKeys[5]) {
				inp.numKeys[5] = false;
				if (_objectsPtrTable[kObjPtrUtil] && _objectsPtrTable[kObjPtrUtil]->o_next) {
					GameObject *tmpObj = _objectsPtrTable[kObjPtrUtil];
					while (tmpObj->o_next) {
						tmpObj = tmpObj->o_next;
					}
					tmpObj->o_next = _objectsPtrTable[kObjPtrUtil];
					_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_child = _objectsPtrTable[kObjPtrUtil]->o_next;
					_objectsPtrTable[kObjPtrUtil]->o_next = 0;
					_objectsPtrTable[kObjPtrUtil] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_child;
					if (_objectsPtrTable[kObjPtrUtil]) {
						_varsTable[23] = _objectsPtrTable[kObjPtrUtil]->objKey;
						if (getMessage(_objectsPtrTable[kObjPtrUtil]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[kObjPtrUtil]->text = (const char *)_tmpMsg.data;
						}
					}
				}
			}
			if (inp.numKeys[3]) {
				inp.numKeys[3] = false;
				if (_objectsPtrTable[9] && _objectsPtrTable[9]->o_next) {
					GameObject *tmpObj = _objectsPtrTable[9];
					while (tmpObj->o_next) {
						tmpObj = tmpObj->o_next;
					}
					tmpObj->o_next = _objectsPtrTable[9];
					_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_child = _objectsPtrTable[9]->o_next;
					_objectsPtrTable[9]->o_next = 0;
					_objectsPtrTable[9] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_child;
					if (_objectsPtrTable[9]) {
						if (getMessage(_objectsPtrTable[9]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[9]->text = (const char *)_tmpMsg.data;
						}
					}
				}
			}
			if (inp.numKeys[2]) {
				inp.numKeys[2] = false;
				if (_objectsPtrTable[7] && _objectsPtrTable[7]->o_next) {
					GameObject *tmpObj = _objectsPtrTable[7];
					while (tmpObj->o_next) {
						tmpObj = tmpObj->o_next;
					}
					tmpObj->o_next = _objectsPtrTable[7];
					_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child = _objectsPtrTable[7]->o_next;
					_objectsPtrTable[7]->o_next = 0;
					_objectsPtrTable[7] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child;
					if (_objectsPtrTable[7]) {
						_varsTable[25] = _objectsPtrTable[7]->objKey;
						if (getMessage(_objectsPtrTable[7]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[7]->text = (const char *)_tmpMsg.data;
						}
						_objectsPtrTable[kObjPtrConrad]->specialData[1][20] = (_objectsPtrTable[7]->specialData[1][22] == 1) ? 5 : 0;
					}
				}
			}
			if (inp.numKeys[1]) {
				inp.numKeys[1] = false;
				if (_objectsPtrTable[10] && _objectsPtrTable[10]->o_next) {
					GameObject *tmpObj = _objectsPtrTable[10];
					while (tmpObj->o_next) {
						tmpObj = tmpObj->o_next;
					}
					tmpObj->o_next = _objectsPtrTable[10];
					_objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_next->o_child = _objectsPtrTable[10]->o_next;
					_objectsPtrTable[10]->o_next = 0;
					_objectsPtrTable[10] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_next->o_child;
					if (_objectsPtrTable[10]) {
						_varsTable[24] = _objectsPtrTable[10]->objKey;
						if (getMessage(_objectsPtrTable[10]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[10]->text = (const char *)_tmpMsg.data;
						}
					}
				}
			}
		}
	}
}

static const struct {
	int obj;
	int var;
} _gameInventoryObjectsVars[] = {
	{ 11, 15 },
	{  8, 23 },
	{  9, -1 },
	{  7, 25 },
	{ 10, 24 }
};

void Game::setupInventoryObjects() {
	GameObject *o = _objectsPtrTable[kObjPtrInventaire]->o_child;
	for (int i = 0; i < ARRAYSIZE(_gameInventoryObjectsVars); ++i) {
		GameObject *o_tmp = o->o_child;
		if (o_tmp) {
			if (_gameInventoryObjectsVars[i].var != -1) {
				_varsTable[_gameInventoryObjectsVars[i].var] = o_tmp->objKey;
			}
			if (getMessage(o_tmp->objKey, 0, &_tmpMsg)) {
				o_tmp->text = (const char *)_tmpMsg.data;
			}
			_objectsPtrTable[_gameInventoryObjectsVars[i].obj] = o_tmp;
		} else {
			warning("Game::setupInventoryObjects() missing inventory object index %d", i);
		}
		if (_gameInventoryObjectsVars[i].obj == 7) {
			assert(o_tmp);
			if (o_tmp->specialData[1][22] == 1) {
				_objectsPtrTable[kObjPtrConrad]->specialData[1][20] = 5;
			}
		}
		o = o->o_next;
	}

	int16_t nextKey, childKey, sprKey;
	GameObjectAnimation *anim = &_objectsPtrTable[kObjPtrInventaire]->anim;
	const uint8_t *p_anifram = anim->aniframData;
	_inventoryBackgroundKey = READ_LE_UINT16(p_anifram);
	nextKey = anim->currentAnimKey;
	for (int i = 0; i < 4; ++i) {
		nextKey = _res.getNext(kResType_ANI, nextKey);
		childKey = _res.getChild(kResType_ANI, nextKey);
		p_anifram = _res.getData(kResType_ANI, childKey, "ANIFRAM");
		_inventoryCursor[i] = READ_LE_UINT16(p_anifram);
	}
	nextKey = _res.getNext(kResType_ANI, nextKey);
	childKey = _res.getChild(kResType_ANI, nextKey);
	p_anifram = _res.getData(kResType_ANI, childKey, "ANIFRAM");
	sprKey = READ_LE_UINT16(p_anifram);
	if (!_infoPanelSpr.data) {
		const uint8_t *p_btmdesc = _res.getData(kResType_SPR, sprKey, "BTMDESC");
		_infoPanelSpr.w = READ_LE_UINT16(p_btmdesc);
		_infoPanelSpr.h = READ_LE_UINT16(p_btmdesc + 2);
		_infoPanelSpr.data = _res.getData(kResType_SPR, sprKey, "SPRDATA");
		assert(_infoPanelSpr.data);
		_infoPanelSpr.data = _spriteCache.getData(sprKey, _infoPanelSpr.data);
		_infoPanelSpr.key = sprKey;
		_res.unload(kResType_SPR, sprKey);
	}

	memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));
	_drawCharBuf.ptr = _infoPanelSpr.data;
	_drawCharBuf.w = _drawCharBuf.pitch = _infoPanelSpr.w;
	_drawCharBuf.h = _infoPanelSpr.h;

	static const int y[] = { 0, 20, 50 };
	for (int i = 0; i < 3; ++i) {
		if (getMessage(_objectsPtrTable[kObjPtrInventaire]->objKey, i, &_tmpMsg)) {
			drawString(8, y[i], (const char *)_tmpMsg.data, _tmpMsg.font, 0);
		}
	}
}

void Game::addParticle(int xPos, int yPos, int zPos, int rnd, int dx, int dy, int dz, int count, int ticks, int fl, int speed) {
	const int particlesLeft = kParticlesTableSize - _particlesCount;
	if (count > particlesLeft) {
		count = particlesLeft;
	}
	assert(_particlesCount + count <= kParticlesTableSize);
	for (int i = 0; i < count; ++i) {
		Particle *part = &_particlesTable[_particlesCount + i];
		memset(part, 0, sizeof(Particle));
		part->xPos = xPos;
		part->yPos = (kGroundY << 15) + yPos;
		part->zPos = zPos;
		part->dx = dx + _rnd.getRandomNumberShift(rnd);
		part->dy = dy - _rnd.getRandomNumberShift(rnd);
		part->dz = dz + _rnd.getRandomNumberShift(rnd);
		part->ticks = ticks + (_rnd.getRandomNumber() >> 9);
		part->fl = fl;
		part->speed = speed;
	}
	_particlesCount += count;
}

void Game::updateParticles() {
	for (int i = 0; i < _particlesCount; ) {
		Particle *part = &_particlesTable[i];
		--part->ticks;
		if (part->ticks <= 0) {
			--_particlesCount;
			if (i < _particlesCount) {
				_particlesTable[i] = _particlesTable[i + 1];
			}
		} else {
			++i;
		}
	}
	for (int i = 0; i < _particlesCount; ++i) {
		Particle *part = &_particlesTable[i];
		part->dy += 1 << 12;
		if (part->yPos >= (kGroundY << 15)) {
			part->yPos = kGroundY << 15;
			part->dy = (-part->dy) >> 2;
			part->dx >>= 1;
			part->dz >>= 1;
		}
		part->xPos += part->dx;
		part->yPos += part->dy;
		part->zPos += part->dz;
	}
}

void Game::drawParticles() {
	for (int i = 0; i < _particlesCount; ++i) {
		Particle *part = &_particlesTable[i];
		int color;
		if (part->fl & 0x8000) {
			color = _mrkBuffer[254 + (part->fl & 255)];
		} else {
			color = _indirectPalette[part->fl & 15][0]; // TODO: pass true color
		}
		Vertex v;
		v.x = part->xPos >> kPosShift;
		v.y = part->yPos >> kPosShift;
		v.z = part->zPos >> kPosShift;
		_render->drawParticle(&v, color);
	}
}

void Game::initSprites() {
	int16_t key = _res.getChild(kResType_ANI, _objectsPtrTable[kObjPtrCible]->anim.currentAnimKey);
	for (int i = 0; i < 2; ++i) {
		const uint8_t *p_anifram = _res.getData(kResType_ANI, key, "ANIFRAM");
		assert(p_anifram);
		_spritesTable[i] = READ_LE_UINT16(p_anifram);
		key = _res.getNext(kResType_ANI, key);
	}
}

void Game::updateScreen() {
	if (_objectsPtrTable[kObjPtrConrad]->specialData[1][18] <= 1) {
		_snd.playSfx(_objectsPtrTable[kObjPtrWorld]->objKey, _res._sndKeysTable[1]);
	} else if (_objectsPtrTable[kObjPtrConrad]->specialData[1][18] < 100 && _objectsPtrTable[kObjPtrConrad]->specialData[1][18] > 1) {
		_snd.playSfx(_objectsPtrTable[kObjPtrWorld]->objKey, _res._sndKeysTable[1]);
	}
	displayTarget(kScreenWidth / 2, kScreenHeight / 2);
	if (_mainLoopCurrentMode == 1) {
		printGameMessages();
	}
	if (_updatePalette) {
		updatePalette();
		_updatePalette = false;
	}
}

void Game::printGameMessages() {
	for (int i = 0; i < kPlayerMessagesTableSize; ++i) {
		GamePlayerMessage *msg = &_playerMessagesTable[i];
		if (msg->desc.duration > 0) {
			msg->visible = false;
			if (1) {
				switch (msg->desc.font & 96) {
				case 32:
					msg->visible = (_ticks & 31) > 16;
					break;
				case 64:
					msg->visible = (_ticks & 15) > 8;
					break;
				case 96:
					msg->visible = (_ticks & 7) > 4;
					if (msg->visible && msg->value == 6) {
						_snd.playSfx(_objectsPtrTable[kObjPtrWorld]->objKey, _res._sndKeysTable[0]);
					}
					break;
				default:
					msg->visible = true;
					break;
				}
				if (msg->visible) {
					assert(msg->desc.data);
					memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));
					drawString(msg->desc.xPos, msg->desc.yPos, (const char *)msg->desc.data, msg->desc.font, 0);
				}
			}
			--msg->desc.duration;
			if (_snd.isVoicePlaying(msg->objKey)) {
				if (msg->desc.duration <= 0) {
					msg->desc.duration = 1;
				}
			}
		} else if (msg->desc.data) {
			memset(msg, 0, sizeof(GamePlayerMessage));
			--_playerMessagesCount;
		}
	}
}

bool Game::sendMessage(int msg, int16_t destObjKey) {
	if (destObjKey == 0) {
		return true;
	}
	CollisionSlot *colSlot = 0;
	GameObject *o = 0;
	if (destObjKey == -1) {
		o = _currentObject;
		const int xMap = _currentObject->xPosParent + _currentObject->xPos;
		const int zMap = _currentObject->zPosParent + _currentObject->zPos;
		colSlot = getCellMapShr19(xMap, zMap)->colSlot;
		if (colSlot) {
			o = colSlot->o;
		}
	} else {
		o = getObjectByKey(destObjKey);
		assert(o);
	}
	if ((_currentObject->flags[1] & 0x80) != 0 && (o->flags[1] & 0x80) != 0 && msg == 57) {
		return false;
	}
	if (_currentObject->specialData[1][23] == msg && msg == 57 && _currentObject != o) {
		return true;
	}
	while (o) {
		if (destObjKey == -1) {
			const int xPos = _currentObject->xPosParent + _currentObject->xPos;
			const int zPos = _currentObject->zPosParent + _currentObject->zPos;
			while (o == _currentObject || (_currentObject->specialData[1][8] & o->specialData[1][8]) == 0 || !testCollisionSlotRect2(_currentObject, o, xPos, zPos)) {
				if (!colSlot->next) {
					return true;
				}
				colSlot = colSlot->next;
				o = colSlot->o;
			}
		}
		const int type = o->specialData[1][21];
		if ((o->flags[1] & 0x100) != 0 || (type != 16 && type != 0x400000 && type != 2 && _currentObject->specialData[1][21] == 8 && msg == 58)) {
			if (msg == 58) {
				_currentObject->specialData[1][18] = -1;
				_varsTable[19] = o->objKey;
			}
			if (destObjKey != -1) {
				return true;
			}
		} else if (o->scriptStateData) {
			bool hasMsg = false;
			int messagesCount = o->scriptStateData[3];
			if (messagesCount != 0) {
				const uint8_t *msgList = _res.getMsgData(o->scriptStateData[2]);
				for (int i = 0; i < messagesCount; ++i) {
					if (msgList[i] == msg) {
						hasMsg = true;
						break;
					}
				}
			}
			if (hasMsg || messagesCount == 0) {
				if ((o->flags[1] & 0x10) != 0 && o->state != 1) {
					o->state = 3;
					addToChangedObjects(o);
				}
				bool alreadyInList = false;
				for (GameMessage *m_cur = o->msg; m_cur; m_cur = m_cur->next) {
					if (m_cur->objKey == _currentObjectKey && m_cur->num == msg) {
						m_cur->ticks = 0;
						alreadyInList = true;
						break;
					}
				}
				if (!alreadyInList) {
					GameMessage *m_new = (GameMessage *)calloc(1, sizeof(GameMessage));
					m_new->next = o->msg;
					o->msg = m_new;
					m_new->objKey = _currentObject->objKey;
					m_new->ticks = 0;
					m_new->num = msg;
					if (msg == 58 && hasMsg) {
						int typeObj = _currentObject->specialData[1][21];
						int specObj = _currentObject->specialData[1][22];
						int lifeCounterObj = _currentObject->specialData[1][18];
						int lifeCounter = o->specialData[1][18];
						if (typeObj == 8 && (specObj == 16 || specObj == 64 || specObj == 128)) {
							GameObject *o_shoot = _objectsPtrTable[kObjPtrConrad];
							const int dx = ((o_shoot->xPosParent + o_shoot->xPos) - (o->xPosParent + o->xPos)) >> kPosShift;
							const int dz = ((o_shoot->zPosParent + o_shoot->zPos) - (o->zPosParent + o->zPos)) >> kPosShift;
							const int distance = dx * dx + dz * dz;
							if (distance <= 576) {
								lifeCounterObj *= 3;
							}
						} else if (typeObj == 8 && specObj == 1) {
							GameObject *o_shoot = getObjectByKey(_currentObject->specialData[1][9]);
							if (o_shoot) {
								const int dx = ((o_shoot->xPosParent + o_shoot->xPos) - (o->xPosParent + o->xPos)) >> kPosShift;
								const int dz = ((o_shoot->zPosParent + o_shoot->zPos) - (o->zPosParent + o->zPos)) >> kPosShift;
								const int distance = dx * dx + dz * dz;
								if (distance <= 576) {
									lifeCounter *= 3;
								}
							}
						}
						bool isConradShoot = (typeObj == 8 && (specObj & 0xDF0) != 0); /* NOTE: different from the original */
						switch (_skillLevel) {
						case kSkillEasy:
							if (o == _objectsPtrTable[kObjPtrConrad]) {
								if (lifeCounterObj < _objectsPtrTable[kObjPtrConrad]->specialData[1][18]) {
									lifeCounterObj = (lifeCounterObj / 2) + 1;
								}
							} else if (isConradShoot) {
								lifeCounterObj *= 2;
							}
							break;
						case kSkillHard:
							if (o == _objectsPtrTable[kObjPtrConrad]) {
								lifeCounterObj *= 2;
							} else if (isConradShoot) {
								lifeCounterObj /= 2;
							}
							break;
						}
						int lifeCounter2 = lifeCounter;
						if (_currentObject->specialData[1][18] >= 0) {
							o->specialData[1][18] -= lifeCounterObj;
						} else {
							o->specialData[1][18] += lifeCounterObj;
						}
						if (lifeCounter2 >= 0) {
							_currentObject->specialData[1][18] -= lifeCounter2;
						} else {
							_currentObject->specialData[1][18] += lifeCounter2;
						}
						if (o->specialData[1][18] <= 0) {
							o->specialData[1][8] = 0;
						}
					}
					const int envp3d = o->specialData[1][20] & 15;
					if (o == _objectsPtrTable[kObjPtrConrad] && msg == 58 && (envp3d == 0 || envp3d == 5)) {
						int x2, z2;
						int x1 = o->xPosParent + o->xPos;
						int z1 = o->zPosParent + o->zPos;
						if (_currentObject->specialData[1][21] == 8 && _currentObject->specialData[1][22] == 1) {
							GameObject *o_tmp = getObjectByKey(_currentObject->specialData[1][9]);
							x2 = o_tmp->xPosParent + o_tmp->xPos;
							z2 = o_tmp->zPosParent + o_tmp->zPos;
						} else {
							x2 = _currentObject->xPosParent + _currentObject->xPos;
							z2 = _currentObject->zPosParent + _currentObject->zPos;
						}
						const int envShoot = o->specialData[1][20] & 255;
						_conradEnvAni = (envShoot >= 1 && envShoot <= 4) ? 0 : o->specialData[1][20];
						int angle = getAngleFromPos(x2 - x1, z2 - z1);
						angle = (angle - (o->pitch & 1023)) & 1023;
						if (angle <= 128 || angle >= 896) {
							o->specialData[1][20] = 1;
						} else if (angle <= 384 && angle > 128) {
							o->specialData[1][20] = 3;
						} else if (angle <= 640 && angle > 384) {
							o->specialData[1][20] = 2;
						} else {
							o->specialData[1][20] = 4;
						}
						const int type = _currentObject->specialData[1][21];
						if (type != 262144 && type != 2048 && type != 8388608) {
							_snd.playSfx(_objectsPtrTable[kObjPtrWorld]->objKey, _res._sndKeysTable[2]);
						}
						if (_conradHit == 0) {
							_conradHit = 2;
						}
					}
					if (o == _objectsPtrTable[kObjPtrConrad] && msg == 58 && o->specialData[1][18] <= 0) {
						playDeathCutscene(_currentObject->objKey);
						_endGame = true;
					}
					debug(kDebug_GAME, "Send message (%s,%d) -> (%s,%d)", _currentObject->name, _currentObject->objKey, o->name, o->objKey);
				}
			}
		}
		if (destObjKey != -1) {
			break;
		}
		colSlot = colSlot->next;
		if (!colSlot) {
			break;
		}
		o = colSlot->o;
	}
	return true;
}

void Game::addToCollidingObjects(GameObject *o) {
	if ((o->flags[1] & 0x100) == 0) {
		o->setColliding = true;
		assert(_collidingObjectsCount < kCollidingObjectsTableSize);
		_collidingObjectsTable[_collidingObjectsCount] = o;
		++_collidingObjectsCount;
	}
}

void Game::updateCollidingObjects() {
	for (int i = 0; i < _collidingObjectsCount; ++i) {
		_currentObject = _collidingObjectsTable[i];
		updateCurrentObjectCollisions();
	}
	_collidingObjectsCount = 0;
}

int Game::getCollidingHorizontalMask(int yPos, int upDir, int downDir) {
	int y2 = (-yPos + (upDir << kPosShift)) >> (kPosShift + 1);
	int y1 = (-yPos - (downDir << kPosShift)) >> (kPosShift + 1);
	y1 = CLIP(y1 + 1, 1, 31);
	y2 = CLIP(y2 + 1, 1, 31);
	int mask = 0;
	for (; y1 <= y2; ++y1) {
		mask |= 1 << y1;
	}
	return mask;
}

void Game::sendShootMessageHelper(GameObject *o, int xPos, int zPos, int radius, int num) {
	int dist = getSquareDistance(o->xPos, xPos, o->zPos, zPos, kPosShift);
	if (dist <= radius) {
		_currentObject->specialData[1][18] = num;
		int type = _currentObject->specialData[1][21];
		_currentObject->specialData[1][21] = 0x20000;
		sendMessage(58, o->objKey);
		_currentObject->specialData[1][21] = type;
	}
}

void Game::initViewport() {
	assert(_viewportSize >= 0 && _viewportSize <= kViewportMax);
	const int scale = (kViewportMax - _viewportSize) * 128 / kViewportMax + 128;
	_render->_viewport.pw = scale;
	_render->_viewport.ph = scale;
	_render->_viewport.changed = true;
}

void Game::drawInfoPanel() {
	const int xPos = 6;
	const int yPos = 140;
	if (_level != 6 && _level != 12) {
		_render->drawSprite(xPos, yPos, _infoPanelSpr.data, _infoPanelSpr.w, _infoPanelSpr.h, _infoPanelSpr.key);
	} else {
// TODO: pitch
//		_render->drawSprite(xPos, yPos, _infoPanelSpr.data, _infoPanelSpr.w, pitch=4, _infoPanelSpr.h);
	}
	const uint8_t color = _indirectPalette[kIndirectColorYellow][1];
	const int life = 48 * _objectsPtrTable[kObjPtrConrad]->specialData[1][18] / _varsTable[kVarConradLife];
	_render->drawRectangle(xPos + 1, yPos + 1 + 48 - life, 2, life, color);

	if (_objectsPtrTable[kObjPtrGun] && _objectsPtrTable[kObjPtrGun]->specialData[1][22] == 4096) {
		const int amount = (48 * _objectsPtrTable[kObjPtrGun]->customData[4]) / _varsTable[kVarConradLife];
		const int h = (amount * 10) / 100;
		_render->drawRectangle(xPos + 1, yPos + 1 + 48 - h, 2, h, 68);
	}

	memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));
	if (_level != 6 && _level != 12) {
		if (_objectsPtrTable[10]) {
			if (_objectsPtrTable[10]->specialData[1][18] > 0) {
				if (_ticks & 1) {
					drawString(xPos + 33, yPos - 2, _objectsPtrTable[10]->text, kFontNameCart, 0);
				}
			} else {
				drawString(xPos + 33, yPos - 2, _objectsPtrTable[10]->text, kFontNameCart, 0);
			}
		}
		if (_objectsPtrTable[7]) {
			if (_objectsPtrTable[7]->specialData[1][22] == 2 || (_objectsPtrTable[kObjPtrConrad]->specialData[1][20] & 15) == 5) {
				if (_ticks & 1) {
					drawString(xPos + 33, yPos + 7, _objectsPtrTable[7]->text, kFontNameCart, 0);
				}
			} else {
				drawString(xPos + 33, yPos + 7, _objectsPtrTable[7]->text, kFontNameCart, 0);
			}
		}
		if (_objectsPtrTable[9]) {
			char buf[64];
			snprintf(buf, sizeof(buf), "%s%d", _objectsPtrTable[9]->text, _objectsPtrTable[9]->customData[0]);
			drawString(xPos + 33, yPos + 18, buf, kFontNameCart, 0);
		}
		if (_objectsPtrTable[kObjPtrGun]) {
			char buf[64];
			const int type = _objectsPtrTable[kObjPtrGun]->specialData[1][22];
			if (type != 4096) {
				snprintf(buf, sizeof(buf), "%d", _objectsPtrTable[kObjPtrGun]->customData[0]);
				drawString(xPos + 11, yPos + 31, buf, kFontNbCart, 0);
			}
			strcpy(buf, _objectsPtrTable[kObjPtrGun]->text);
			if (type != 64 && type != 512 && type != 4096) {
				int count = _objectsPtrTable[kObjPtrGun]->customData[1] / 9;
				if ((_objectsPtrTable[kObjPtrGun]->customData[1] % 9) != 0) {
					count++;
				}
				char str[16];
				snprintf(str, sizeof(str), "%d", count);
				strcat(buf, str);
			}
			drawString(xPos + 33, yPos + 27, buf, kFontNameCart, 0);
		}
		if (_objectsPtrTable[8]) {
			drawString(xPos + 33, yPos + 48, _objectsPtrTable[8]->text, kFontNameCart, 0);
		}
	}
}

void Game::getCutsceneMessages(int num) {
	_cutsceneMessagesCount = 0;
	GameObject *o = _objectsPtrTable[kObjPtrScenesCine];
	assert(o);
	for (o = o->o_child; o; o = o->o_next) {
		if (o->customData[0] == num) {
			GamePlayerMessage *msg = &_cutsceneMessagesTable[0];
			for (int ref = 0; getMessage(o->objKey, ref, &msg->desc); ++ref) {
				if (msg->desc.font & 128) {
					char name[32];
					snprintf(name, sizeof(name), "%s_%d", o->name, ref);
					msg->crc = getStringHash(name);
				}
				++_cutsceneMessagesCount;
				assert(_cutsceneMessagesCount < kCutsceneMessagesTableSize);
				msg = &_cutsceneMessagesTable[_cutsceneMessagesCount];
			}
			break;
		}
	}
}

void Game::playDeathCutscene(int objKey) {
	GameObject *o = getObjectByKey(objKey);
	if (o->specialData[1][21] == 0x40000) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 48;
		_level = -14;
	} else if (_level == 6) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 9;
	} else if (_level == 12) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 33;
	} else if (o->specialData[1][21] == 0x1000000) {
		_cut._numToPlayCounter = 4;
		_cut._numToPlay = 1;
	} else if (o->specialData[1][21] == 8) {
		switch (o->specialData[1][22]) {
		case 0x1:
			_cut._numToPlayCounter = 4;
			o = getObjectByKey(o->specialData[1][9]);
			if (o->specialData[1][22] != 0x4000) {
				_cut._numToPlay = 1;
			} else {
				_cut._numToPlay = 6;
			}
			break;
		case 0x2:
		case 0x4000:
			_cut._numToPlayCounter = 4;
			_cut._numToPlay = 2;
			break;
		case 0x200:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 4;
			break;
		default:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 12;
			break;
		}
	} else if (o->specialData[1][21] == 16) {
		switch (o->specialData[1][22]) {
		case 0x4:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 0;
			break;
		case 0x20:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 3;
			break;
		case 0x80000:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 2;
			break;
		case 0x100000:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 17;
			break;
		case 0x200000:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 7;
			break;
		case 0x1000000:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 40;
			break;
		case 0x4000000:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 11;
			break;
		case 0x800000:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 42;
			break;
		default:
			_cut._numToPlayCounter = 0;
			_cut._numToPlay = 12;
			break;
		}
	} else if (o->specialData[1][21] == 0x20000) {
		_cut._numToPlayCounter = 4;
		_cut._numToPlay = 2;
	} else if (o->specialData[1][21] == 0x200000) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 41;
	} else if (o->specialData[1][21] == 0x10000) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 10;
	} else if (o->specialData[1][21] == 0x100) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 5;
	} else if (o->specialData[1][21] == 0x800) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 17;
	} else if (o->specialData[1][21] == 0x800000) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 49;
	} else if (o->specialData[1][21] == 0x100000) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 34;
	} else if (o->specialData[1][21] == 0x8000) {
		_cut._numToPlayCounter = 4;
		_cut._numToPlay = 8;
	} else if (o->specialData[1][21] == 0x80000) {
		_cut._numToPlayCounter = 4;
		_cut._numToPlay = 38;
	} else if (o->specialData[1][21] == 0x2000000) {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 3;
	} else {
		_cut._numToPlayCounter = 0;
		_cut._numToPlay = 12;
	}
}

void Game::displayTarget(int cx, int cy) {
	if (_varsTable[12] != 0 && _varsTable[13] > 0) {
		GameObject *o = getObjectByKey(_varsTable[12]);
		const int xPosTarget = o->xPosParent + o->xPos;
		const int zPosTarget = o->zPosParent + o->zPos;
		_objectsPtrTable[kObjPtrMusic] = o;
		if (_snd._musicMode != 3) {
			playMusic(3);
		}
		const int pitch = (-_yRotObserver) & 1023;
		GameObject *objconrad = _objectsPtrTable[kObjPtrConrad];
		const int xPos_conrad = objconrad->xPosParent + objconrad->xPos;
		const int zPos_conrad = objconrad->zPosParent + objconrad->zPos;
		CellMap *cell = getCellMapShr19(xPosTarget, zPosTarget);
		const int room = (cell->room != 0) ? cell->room : cell->room2;
		cell = getCellMapShr19(xPos_conrad, zPos_conrad);
		const int room1_conrad = cell->room;
		const int room2_conrad = cell->room2;
		if (o->state == 1 && o->o_parent != _objectsPtrTable[kObjPtrCimetiere] && (room == room1_conrad || room == room2_conrad)) {
			const uint8_t *p_btm0 = _res.getData(kResType_SPR, _spritesTable[0], "BTMDESC");
			const int spr0_w = READ_LE_UINT16(p_btm0);
			const int spr0_h = READ_LE_UINT16(p_btm0 + 2);
			const uint8_t *p_spr0 = _res.getData(kResType_SPR, _spritesTable[0], "SPRDATA");
			p_spr0 = _spriteCache.getData(_spritesTable[0], p_spr0);
			if (p_spr0) {
				_render->drawSprite(cx - spr0_w / 2, cy - spr0_h / 2, p_spr0, spr0_w, spr0_h, _spritesTable[0]);
			}
			const int r = spr0_w / 2;
			int a = getAngleFromPos(xPosTarget - _xPosObserver, zPosTarget - _zPosObserver);
			a = (a - (pitch & 1023)) & 1023;
			int cosa = g_cos[a];
			int sina = g_sin[a];
			const int tx = ( sina * r) >> 15;
			const int ty = (-cosa * r) >> 15;
			const uint8_t *p_btm1 = _res.getData(kResType_SPR, _spritesTable[1], "BTMDESC");
			const int spr1_w = READ_LE_UINT16(p_btm1);
			const int spr1_h = READ_LE_UINT16(p_btm1 + 2);
			const uint8_t *p_spr1 = _res.getData(kResType_SPR, _spritesTable[1], "SPRDATA");
			p_spr1 = _spriteCache.getData(_spritesTable[1], p_spr1);
			if (p_spr1) {
				_render->drawSprite(cx + tx - spr1_w / 2, cy + ty - spr1_h / 2, p_spr1, spr1_w, spr1_h, _spritesTable[1]);
			}
			--_varsTable[13];
			if (_varsTable[13] <= 0) {
				_varsTable[12] = 0;
			}
		} else {
			int32_t flag = -1;
			op_clearTarget(1, &flag);
		}
	}
}

int Game::getShootPos(int16_t objKey, int *x, int *y, int *z) {
	GameObject *o = (objKey == 0) ? _currentObject : getObjectByKey(objKey);
	if (!o) {
		return 0;
	}
	GameObjectAnimation *anim = &o->anim;
	uint8_t *p_anikeyf = anim->anikeyfData;
	if (anim->currentAnimKey == 0) {
		return 0;
	}
	p_anikeyf = _res.getData(kResType_ANI, anim->currentAnimKey, "ANIKEYF");
	const int xb = ((p_anikeyf[16] << 3) + READ_LE_UINT16(p_anikeyf + 2)) << 10;
	const int zb = ((p_anikeyf[17] << 3) + READ_LE_UINT16(p_anikeyf + 6)) << 10;
	int cosy =  g_cos[o->pitch & 1023];
	int siny = -g_sin[o->pitch & 1023];
	const int rx = fixedMul(cosy, xb, 15) - fixedMul(siny, zb, 15);
	const int rz = fixedMul(siny, xb, 15) + fixedMul(cosy, zb, 15);
	*x = o->xPosParent + o->xPos + rx;
	*y = o->yPosParent + o->yPos + (READ_LE_UINT16(p_anikeyf + 14) << 11);
	*z = o->zPosParent + o->zPos + rz;
	return -1;
}

void Game::drawSprite(int x, int y, int sprKey) {
	const uint8_t *p_btmdesc = _res.getData(kResType_SPR, sprKey, "BTMDESC");
	const int w = READ_LE_UINT16(p_btmdesc);
	const int h = READ_LE_UINT16(p_btmdesc + 2);
	const uint8_t *data = _res.getData(kResType_SPR, sprKey, "SPRDATA");
	data = _spriteCache.getData(sprKey, data);
	if (data) {
		_render->copyToOverlay(x, y, data, w, w, h, 0);
	}
}
