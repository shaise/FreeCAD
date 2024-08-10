/***************************************************************************
 *   Copyright (c) 2005 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/nodes/SoCube.h>
#endif

#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoTransparencyType.h>

#include "SoFC3DEffectsSetup.h"
#include "View3DInventorViewer.h"


using namespace Gui;

// *************************************************************************
extern char const TxtVertexShader[];
extern char const TxtShadowFragmentShader[];

#define SHADOW_TEX_W  512
#define SHADOW_TEX_H  512

static void XSectionPrepareCut(void* data, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        SoFC3DEffectsSetup* effectsetup = (SoFC3DEffectsSetup*)data;
        effectsetup->updateGeometry();
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glCullFace(GL_BACK);
        glDepthFunc(GL_LESS);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glPolygonOffset(1, 2);
        glEnable(GL_POLYGON_OFFSET_FILL);
        effectsetup->HideLines();
    }
}

static void XSectionDrawBack(void* data, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glCullFace(GL_FRONT);
    }
}

static void XSectionRenderScene(void* data, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        SoFC3DEffectsSetup* effectsetup = (SoFC3DEffectsSetup*)data;
        effectsetup->updateGeometry();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_STENCIL_TEST);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        //glClear(GL_DEPTH_BUFFER_BIT);
        //glDepthFunc(GL_LEQUAL);
        //glDisable(GL_DEPTH_TEST);
        glPolygonOffset(0, 0);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_CULL_FACE);
        effectsetup->RestoreLinesState();
    }
}



SO_NODE_SOURCE(SoFC3DEffectsSetup)

SoFC3DEffectsSetup::SoFC3DEffectsSetup()
{
    //SO_NODE_INTERNAL_CONSTRUCTOR(SoGeoSeparator);

    SO_NODE_CONSTRUCTOR(SoFC3DEffectsSetup);

    SO_NODE_ADD_FIELD(documentName, (""));
    SO_NODE_ADD_FIELD(objectName, (""));
    SO_NODE_ADD_FIELD(subElementName, (""));
    SO_NODE_ADD_FIELD(enableCrossSections, (false));
    SO_NODE_ADD_FIELD(enableAmbientOclusion, (false));
    SO_NODE_ADD_FIELD(enableShadows, (false));

    createScene();
}

void
SoFC3DEffectsSetup::initClass()
{
    SO_NODE_INIT_CLASS(SoFC3DEffectsSetup, SoGroup, "3DEffectsSetup");
}

void SoFC3DEffectsSetup::setScene(SoNode* scene)
{
    if (scene == nullptr) {
        scene = nullScene;
    }

    // we need to replace 2 occurrences of scene
    XSectionRoot->replaceChild(Scene, scene);
    XSectionRoot->replaceChild(Scene, scene);
    Scene = scene;
}

void Gui::SoFC3DEffectsSetup::setRenderManager(SoRenderManager* renderManager)
{
    RenderManager = renderManager;
}

void Gui::SoFC3DEffectsSetup::SetViewer(View3DInventorViewer* viewer)
{
    Viewer3D = viewer;
}

void Gui::SoFC3DEffectsSetup::HideLines()
{
    saveRenderMode = "Flat Lines";
    //Viewer3D->getOverrideMode();
    std::string shadedmode = "Shaded";
    Viewer3D->setOverrideMode(shadedmode);
}

void Gui::SoFC3DEffectsSetup::RestoreLinesState()
{
    Viewer3D->setOverrideMode(saveRenderMode);
}

void SoFC3DEffectsSetup::createScene()
{
    // a cross section hole filler
    createCrossSectionSetup();
    XSectionSwitch = new SoSwitch;
    XSectionSwitch->whichChild = 0;
    XSectionSwitch->addChild(XSectionRoot);
    addChild(XSectionSwitch);
}

// move function to SoFC3DEffects.cpp
void Gui::SoFC3DEffectsSetup::createSlicerObject()
{
    float vertices[4][3] = {{-100, 0, 0},
                            { 0, 0, -100},
                            { 100, 0,  0},
                            { 0, 0,  100}};

    static float texCoordVals[4][2] = {{0, 0}, {50, 0}, {50, 50}, {0, 50}};

    unsigned char texture[16] =
        {255, 255, 255, 255, 255, 235, 215, 215, 215, 215, 235, 255, 255, 255, 255};

    sliceObject = new SoSeparator;


    SoTextureCoordinate2* texCoords = new SoTextureCoordinate2;
    texCoords->point.setValues(0, 4, texCoordVals);
    SoTextureCoordinateBinding* texBinding = new SoTextureCoordinateBinding;
    texBinding->value = SoTextureCoordinateBinding::PER_VERTEX;
    SoTexture2* textureNode = new SoTexture2;
    textureNode->wrapS = SoTexture2::REPEAT;
    textureNode->wrapT = SoTexture2::REPEAT;
    textureNode->image.setValue(SbVec2s(14, 1), 1, &texture[1]);
    SoComplexity* complexity = new SoComplexity;
    complexity->textureQuality = 1.0;
    sliceObject->addChild(complexity);
    sliceObject->addChild(texCoords);
    sliceObject->addChild(texBinding);
    sliceObject->addChild(textureNode);

    auto planeCoords = new SoCoordinate3;
    planeCoords->point.setValues(0, 4, vertices);
    sliceObject->addChild(planeCoords);

    SoFaceSet* faceSet = new SoFaceSet;
    faceSet->numVertices.set1Value(0, 4);  // One quad with 4 vertices
    sliceObject->addChild(faceSet);
    
    //auto cube = new SoCube;
    //cube->width = 100;
    //cube->height = 10;
    //cube->depth = 100;
    //sliceObject->addChild(cube);
}

bool SoFC3DEffectsSetup::updateBoundingBox()
{
    SbViewportRegion myViewport;
    SoGetBoundingBoxAction object_bbox(myViewport);
    object_bbox.apply(Scene);
    BoundingBox = object_bbox.getBoundingBox();
    if (BoundingBox == OldBoundingBox) {
        return false;
    }
    OldBoundingBox = BoundingBox;

    float sizex, sizey, sizez;
    BoundingBox.getSize(sizex, sizey, sizez);
    float sizexy = sizex > sizey ? sizex : sizey;
    float padz = sizexy * 0.1;
    sizexy *= 1.4 / 2.0;

    float centx = BoundingBox.getCenter()[0];
    float centy = BoundingBox.getCenter()[1];

    SbVec3f& minp = BoundingBox.getMin();
    SbVec3f& maxp = BoundingBox.getMax();
    BoundingBox.setBounds(centx - sizexy, centy - sizexy, minp[2] - padz, 
        centx + sizexy, centy + sizexy, maxp[2] + padz);
    return true;
}


void SoFC3DEffectsSetup::updateGeometry()
{}

SoSeparator* Gui::SoFC3DEffectsSetup::getSlicerObject()
{
    return sliceObject;
}

void Gui::SoFC3DEffectsSetup::createCrossSectionSetup()
{
    XSectionRoot = new SoSeparator;
    nullScene = new SoSeparator;
    nullScene->ref();
    Scene = nullScene;
    createSlicerObject();

    auto prepareCutCallback = new SoCallback;
    prepareCutCallback->setCallback(XSectionPrepareCut, this);
    XSectionRoot->addChild(prepareCutCallback);

    XSectionRoot->addChild(Scene);

    auto drawBackCallback = new SoCallback;
    drawBackCallback->setCallback(XSectionDrawBack, this);
    XSectionRoot->addChild(drawBackCallback);

    XSectionRoot->addChild(Scene);

    auto renderSceneCallback = new SoCallback;
    renderSceneCallback->setCallback(XSectionRenderScene, this);
    XSectionRoot->addChild(renderSceneCallback);
}

SoFC3DEffectsSetup::~SoFC3DEffectsSetup()
{
    nullScene->unref();
}

