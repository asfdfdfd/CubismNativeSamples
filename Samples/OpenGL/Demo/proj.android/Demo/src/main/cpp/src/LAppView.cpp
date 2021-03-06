﻿/*
* Copyright(c) Live2D Inc. All rights reserved.
*
* Use of this source code is governed by the Live2D Open Software license
* that can be found at http://live2d.com/eula/live2d-open-software-license-agreement_en.html.
*/

#include "LAppView.hpp"
#include <math.h>
#include <string>
#include "LAppPal.hpp"
#include "LAppDelegate.hpp"
#include "LAppLive2DManager.hpp"
#include "LAppTextureManager.hpp"
#include "LAppDefine.hpp"
#include "TouchManager.hpp"
#include "LAppSprite.hpp"
#include "../JniBridgeC.hpp"

using namespace std;
using namespace LAppDefine;

LAppView::LAppView():
    _programId(0),
    _back(NULL),
    _gear(NULL),
    _power(NULL),
    _changeModel(false)
{
    // タッチ関係のイベント管理
    _touchManager = new TouchManager();

    // デバイス座標からスクリーン座標に変換するための
    _deviceToScreen = new CubismMatrix44();

    // 画面の表示の拡大縮小や移動の変換を行う行列
    _viewMatrix = new CubismViewMatrix();
}

LAppView::~LAppView()
{ 
    delete _viewMatrix;
    delete _deviceToScreen;
    delete _touchManager;
    delete _back;
    delete _gear;
    delete _power;
}

void LAppView::Initialize()
{
    int width = LAppDelegate::GetInstance()->GetWindowWidth();
    int height = LAppDelegate::GetInstance()->GetWindowHeight();

    float ratio = static_cast<float>(height) / static_cast<float>(width);
    float left = ViewLogicalLeft;
    float right = ViewLogicalRight;
    float bottom = -ratio;
    float top = ratio;

    _viewMatrix->SetScreenRect(left, right, bottom, top); // デバイスに対応する画面の範囲。 Xの左端, Xの右端, Yの下端, Yの上端

    float screenW = fabsf(left - right);
    _deviceToScreen->LoadIdentity();
    _deviceToScreen->ScaleRelative(screenW / width, -screenW / width);
    _deviceToScreen->TranslateRelative(-width * 0.5f, -height * 0.5f);

    // 表示範囲の設定
    _viewMatrix->SetMaxScale(ViewMaxScale); // 限界拡大率
    _viewMatrix->SetMinScale(ViewMinScale); // 限界縮小率

    // 表示できる最大範囲
    _viewMatrix->SetMaxScreenRect(
        ViewLogicalMaxLeft,
        ViewLogicalMaxRight,
        ViewLogicalMaxBottom,
        ViewLogicalMaxTop
    );
}

void LAppView::InitializeShader()
{
    _programId = LAppDelegate::GetInstance()->CreateShader();
}

void LAppView::InitializeSprite()
{
    int width = LAppDelegate::GetInstance()->GetWindowWidth();
    int height = LAppDelegate::GetInstance()->GetWindowHeight();

    LAppTextureManager* textureManager = LAppDelegate::GetInstance()->GetTextureManager();
    const string resourcesPath = ResourcesPath;

    string imageName = BackImageName;
    LAppTextureManager::TextureInfo* backgroundTexture = textureManager->CreateTextureFromPngFile(resourcesPath + imageName);

    float x = width * 0.5f;
    float y = height * 0.5f;
    float fWidth = (backgroundTexture->width * 2.0f);
    float fHeight = (height * 0.95f);

    if(_back == NULL)
    {
        _back = new LAppSprite(x, y, fWidth, fHeight, backgroundTexture->id);
    }
    else
    {
        _back->ReSize(x, y, fWidth, fHeight);
    }


    imageName = GearImageName;
    LAppTextureManager::TextureInfo* gearTexture = textureManager->CreateTextureFromPngFile(resourcesPath + imageName);

    x = (width - gearTexture->width * 0.5f - 96.f);
    y = (height - gearTexture->height * 0.5f);
    fWidth = static_cast<float>(gearTexture->width);
    fHeight = static_cast<float>(gearTexture->height);

    if(_gear == NULL)
    {
        _gear = new LAppSprite(x, y, fWidth, fHeight, gearTexture->id);
    }
    else
    {
        _gear->ReSize(x, y, fWidth, fHeight);
    }

    imageName = PowerImageName;
    LAppTextureManager::TextureInfo* powerTexture = textureManager->CreateTextureFromPngFile(resourcesPath + imageName);

    x = (width - powerTexture->width * 0.5f - 96.f);
    y = (powerTexture->height * 0.5f);
    fWidth = static_cast<float>(powerTexture->width);
    fHeight = static_cast<float>(powerTexture->height);

    if(_power == NULL)
    {
        _power = new LAppSprite(x, y, fWidth, fHeight, powerTexture->id);
    }
    else
    {
        _power->ReSize(x, y, fWidth, fHeight);
    }

}

void LAppView::Render()
{
    _back->Render(_programId);
    _gear->Render(_programId);
    _power->Render(_programId);

    if(_changeModel)
    {
        _changeModel = false;
        LAppLive2DManager::GetInstance()->NextScene();
    }

    LAppLive2DManager::GetInstance()->OnUpdate();

}

void LAppView::OnTouchesBegan(float pointX, float pointY) const
{
    _touchManager->TouchesBegan(pointX, pointY);
}

void LAppView::OnTouchesMoved(float pointX, float pointY) const
{
    float viewX = this->TransformViewX(_touchManager->GetX());
    float viewY = this->TransformViewY(_touchManager->GetY());

    _touchManager->TouchesMoved(pointX, pointY);

    LAppLive2DManager::GetInstance()->OnDrag(viewX, viewY);
}

void LAppView::OnTouchesEnded(float pointX, float pointY)
{
    // タッチ終了
    LAppLive2DManager* live2DManager = LAppLive2DManager::GetInstance();
    live2DManager->OnDrag(0.0f, 0.0f);
    {

        // シングルタップ
        float x = _deviceToScreen->TransformX(_touchManager->GetX()); // 論理座標変換した座標を取得。
        float y = _deviceToScreen->TransformY(_touchManager->GetY()); // 論理座標変換した座標を取得。
        if (DebugTouchLogEnable)
        {
            LAppPal::PrintLog("[APP]touchesEnded x:%.2f y:%.2f", x, y);
        }
        live2DManager->OnTap(x, y);

        // 歯車にタップしたか
        if (_gear->IsHit(pointX, pointY))
        {
            _changeModel = true;
        }

        // 電源ボタンにタップしたか
        if (_power->IsHit(pointX, pointY))
        {
            LAppDelegate::GetInstance()->DeActivateApp();
        }
    }
}

float LAppView::TransformViewX(float deviceX) const
{
    float screenX = _deviceToScreen->TransformX(deviceX); // 論理座標変換した座標を取得。
    return _viewMatrix->InvertTransformX(screenX); // 拡大、縮小、移動後の値。
}

float LAppView::TransformViewY(float deviceY) const
{
    float screenY = _deviceToScreen->TransformY(deviceY); // 論理座標変換した座標を取得。
    return _viewMatrix->InvertTransformY(screenY); // 拡大、縮小、移動後の値。
}

float LAppView::TransformScreenX(float deviceX) const
{
    return _deviceToScreen->TransformX(deviceX);
}

float LAppView::TransformScreenY(float deviceY) const
{
    return _deviceToScreen->TransformY(deviceY);
}