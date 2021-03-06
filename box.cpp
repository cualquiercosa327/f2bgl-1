/*
 * Fade To Black engine rewrite
 * Copyright (C) 2006-2012 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "game.h"

void Game::initBox() {
	assert(_boxItemObj);
	setPalette(_palKeysTable[_boxItemObj->pal]);
	loadInventoryObjectMesh(_boxItemObj);
}

void Game::doBox() {
	_render->clearScreen();
	if (_boxItemCount != 0) {

		_render->setupProjection(kProjMenu);
		SceneObject *so = &_sceneObjectsTable[0];
		_render->beginObjectDraw(so->x, so->y, so->z, so->pitch);
		drawSceneObjectMesh(so->polygonsData, so->verticesData, so->verticesCount);
		_render->endObjectDraw();
		so->pitch += 6;
		so->pitch &= 1023;

		const int num = (_boxItemCount == 1) ? 28 : 29;
		if (getMessage(_objectsPtrTable[kObjPtrWorld]->objKey, num, &_tmpMsg)) {
			char buf[128];
			snprintf(buf, sizeof(buf), "%d %s", _boxItemCount, (const char *)_tmpMsg.data);
			memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));
			int w, h;
			getStringRect(buf, kFontNameInvent, &w, &h);
			drawString((kScreenWidth - w) / 2, kScreenHeight - kScreenHeight / 10, buf, kFontNameInvent, 0);
		}

		if (getMessage(_boxItemObj->objKey, 1, &_tmpMsg)) {
			memset(&_drawCharBuf, 0, sizeof(_drawCharBuf));
			int w, h;
			getStringRect((const char *)_tmpMsg.data, _tmpMsg.font, &w, &h);
			drawString((kScreenWidth - w) / 2, 3, (const char *)_tmpMsg.data, _tmpMsg.font, 0);
		}
	}
	if (_boxItemCount > 1) {
		if (inp.dirMask & kInputDirLeft) {
			inp.dirMask &= ~kInputDirLeft;
			_boxItemObj = getPreviousObject(_boxItemObj);
			initBox();
		}
		if (inp.dirMask & kInputDirRight) {
			inp.dirMask &= ~kInputDirRight;
			_boxItemObj = getNextObject(_boxItemObj);
			initBox();
		}
	}
	if (inp.spaceKey) {
		inp.spaceKey = false;
		if (_boxItemCount > 0) {
			--_boxItemCount;
			GameObject *nextItemObj = getNextObject(_boxItemObj);
			GameObject *tmpObj = getObjectByKey(_boxItemObj->customData[0]);
			const int num = tmpObj->specialData[1][22];
			if (num == 2 || num == 8 || num == 16) {
				setObjectParent(_boxItemObj, tmpObj);
				if (!tmpObj->o_child) {
					switch (num) {
					case 2:
						_objectsPtrTable[kObjPtrUtil] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_child;
						_varsTable[23] = _objectsPtrTable[kObjPtrUtil]->objKey;
						if (getMessage(_objectsPtrTable[kObjPtrUtil]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[kObjPtrUtil]->text = (const char *)_tmpMsg.data;
						}
						break;
					case 8:
						_objectsPtrTable[7] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_child;
						_varsTable[25] = _objectsPtrTable[7]->objKey;
						if (getMessage(_objectsPtrTable[7]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[7]->text = (const char *)_tmpMsg.data;
						}
						break;
					case 16:
						_objectsPtrTable[10] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_next->o_next->o_child;
						_varsTable[24] = _objectsPtrTable[10]->objKey;
						if (getMessage(_objectsPtrTable[10]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
							_objectsPtrTable[10]->text = (const char *)_tmpMsg.data;
						}
						break;
					}
				}
			} else if (num == 32) {
				setObjectParent(_boxItemObj, tmpObj);
			} else {
				if (tmpObj->o_parent->o_parent->specialData[1][21] != 128) {
					if (num == 8192 || num == 16384 || num == 32768 || num == 65536) {
						setObjectParent(tmpObj, getObjectByKey(tmpObj->customData[1]));
						_objectsPtrTable[9] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_next->o_next->o_child;
						_objectsPtrTable[9] = tmpObj;
						if (_objectsPtrTable[9]) {
							if (getMessage(_objectsPtrTable[9]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
								_objectsPtrTable[9]->text = (const char *)_tmpMsg.data;
							}
						}
					} else {
						setObjectParent(tmpObj, getObjectByKey(tmpObj->customData[3]));
						_objectsPtrTable[11] = _objectsPtrTable[kObjPtrInventaire]->o_child->o_child;
						if (_objectsPtrTable[11]) {
							_varsTable[15] = _objectsPtrTable[11]->objKey;
							if (getMessage(_objectsPtrTable[11]->objKey, 0, &_tmpMsg) && _tmpMsg.data) {
								_objectsPtrTable[11]->text = (const char *)_tmpMsg.data;
							}
						}
					}
				}
				if (tmpObj->o_parent->specialData[1][22] == 1) {
					tmpObj->customData[1] += _boxItemObj->specialData[1][18];
				} else {
					tmpObj->customData[0] += _boxItemObj->specialData[1][18];
				}
				setObjectParent(_boxItemObj, _objectsPtrTable[kObjPtrCimetiere]);
			}
			_boxItemObj = nextItemObj;
			initBox();
			_snd.playSfx(_objectsPtrTable[kObjPtrWorld]->objKey, _res._sndKeysTable[8]);
		}
	}
	if (inp.escapeKey) {
		inp.escapeKey = false;
		_boxItemCount = 0;
	}
}
