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
#endif

#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoTransparencyType.h>
#include <Inventor/nodes/SoPickStyle.h>

#include "SoFC3DEffects.h"
#include "View3DInventorViewer.h"

using namespace Gui;

// *************************************************************************
extern char const TxtVertexShader[];
extern char const TxtShadowFragmentShader[];

#define SHADOW_TEX_W  512
#define SHADOW_TEX_H  512

static void UpdateCallbackFunc(void* data, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        SoFC3DEffects* effects = (SoFC3DEffects*)data;
        effects->updateGeometry();
        //glCullFace(GL_FRONT);
        // SoCacheElement::invalidate(action->getState());
    }
}

static void XSectionDrawFiller(void* data, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        glDisable(GL_CULL_FACE);
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        //glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        SoFC3DEffects* effects = (SoFC3DEffects*)data;

        // SoCacheElement::invalidate(action->getState());
    }
}


static void XSectionRestoreFunc(void* data, SoAction* action)
{
    if (action->isOfType(SoGLRenderAction::getClassTypeId())) {
        //glCullFace(GL_BACK);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_CULL_FACE);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // SoCacheElement::invalidate(action->getState());
    }
}

SO_NODE_SOURCE(SoFC3DEffects)

SoFC3DEffects::SoFC3DEffects()
{
    //SO_NODE_INTERNAL_CONSTRUCTOR(SoGeoSeparator);

    SO_NODE_CONSTRUCTOR(SoFC3DEffects);

    SO_NODE_ADD_FIELD(documentName, (""));
    SO_NODE_ADD_FIELD(objectName, (""));
    SO_NODE_ADD_FIELD(subElementName, (""));
    SO_NODE_ADD_FIELD(enableBasePlaneShadow, (false));
    SO_NODE_ADD_FIELD(enableAmbientOclusion, (false));
    SO_NODE_ADD_FIELD(enableShadows, (false));

    createScene();
}

void
SoFC3DEffects::initClass()
{
    SO_NODE_INIT_CLASS(SoFC3DEffects, SoSeparator, "3DEffects");
}


bool Gui::SoFC3DEffects::areEffectsSupported()
{
#ifndef GL_MAJOR_VERSION
    return false;
#else
    GLint majorVersion = 0, minorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

    return (majorVersion > 3 || (majorVersion == 3 && minorVersion >= 3));
#endif
}


void SoFC3DEffects::setScene(SoNode* scene)
{
    //if (scene == nullptr) {
    //    scene = nullScene;
    //}
    //XSectionRoot->replaceChild(Scene, scene);

    Scene = scene;
    //updateGeometry();
    if (BaseShadowScene->getNumChildren() > 0)
        BaseShadowScene->removeAllChildren();
    BaseShadowScene->addChild(OrthoCam);
    BaseShadowScene->addChild(Scene);
}

void Gui::SoFC3DEffects::setRenderManager(SoRenderManager* renderManager)
{
    RenderManager = renderManager;
}


void SoFC3DEffects::doDebug()
{
    enableBasePlaneShadow = true;
}

void SoFC3DEffects::createScene()
{
    nullScene = new SoSeparator;
    nullScene->ref();
    Scene = nullScene;

    createShadow();

    auto pickStyle = new SoPickStyle;
    pickStyle->style = SoPickStyle::UNPICKABLE;
    addChild(pickStyle);

    UpdateCallback = new SoCallback;
    UpdateCallback->setCallback(UpdateCallbackFunc, this);
    addChild(UpdateCallback);

    auto transparancyType = new SoTransparencyType;
    transparancyType->value = SoTransparencyType::BLEND;
    addChild(transparancyType);

    // a transparent plane under the scene to accept the shadow
    createShadowPlane();
    shadowSwitch = new SoSwitch;
    shadowSwitch->whichChild = SO_SWITCH_NONE;
    shadowSwitch->addChild(ShadowPlaneRoot);
    addChild(shadowSwitch);

    // a cross section hole filler
    createCrossSection();
    XSectionSwitch = new SoSwitch;
    XSectionSwitch->whichChild = SO_SWITCH_NONE;
    XSectionSwitch->addChild(XSectionRoot);
    addChild(XSectionSwitch);
}

bool SoFC3DEffects::updateBoundingBox()
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

void SoFC3DEffects::updateCameraView()
{
    float sizex, sizey, sizez;
    BoundingBox.getSize(sizex, sizey, sizez);
    SbVec3f cent = BoundingBox.getCenter();
    float maxz = BoundingBox.getMax()[2];
    OrthoCam->height = sizex > sizey ? sizex : sizey;
    OrthoCam->aspectRatio = 1;
    OrthoCam->nearDistance = 0;
    OrthoCam->farDistance = sizez;
    OrthoCam->viewportMapping = OrthoCam->LEAVE_ALONE;
    OrthoCam->position = SbVec3f(cent[0], cent[1], maxz);
    OrthoCam->pointAt(SbVec3f(cent[0], cent[1], cent[2]), SbVec3f(0, 1, 0));
}

void SoFC3DEffects::updatePlaneCoords()
{
    SbVec3f& minp = BoundingBox.getMin();
    SbVec3f& maxp = BoundingBox.getMax();
    float z = minp[2];

    float vertices[4][3] = {
        { minp[0], minp[1], z},
        { maxp[0], minp[1], z},
        { maxp[0], maxp[1], z},
        { minp[0], maxp[1], z}
    };
    ShadowPlaneCoords->point.setValues(0, 4, vertices);
}

void SoFC3DEffects::updateGeometry()
{
    if (!areEffectsSupported()) {
        return;
    }

    if (updateBoundingBox()) {
        updateCameraView();
        updatePlaneCoords();
        if (BoundingBox.isEmpty()) {
            shadowSwitch->whichChild = SO_SWITCH_NONE;
            XSectionSwitch->whichChild = SO_SWITCH_NONE;
        }
        else {
            //shadowSwitch->whichChild = 0;
            XSectionSwitch->whichChild = 0;
        }
        SoCamera* cam = RenderManager->getCamera();
    }
}

void SoFC3DEffects::SetViewer(View3DInventorViewer* viewer)
{
    Viewer3D = viewer;
}

void SoFC3DEffects::createShadow()
{
    BaseShadowTexture = new SoSceneTexture2;
    BaseShadowScene = new SoSeparator;
    BaseShadowScene->ref();

    OrthoCam = new SoOrthographicCamera;

    BaseShadowTexture->scene = BaseShadowScene;
    BaseShadowTexture->size.setValue(SHADOW_TEX_W, SHADOW_TEX_H);
    BaseShadowTexture->model = SoTexture2::DECAL;
}

void SoFC3DEffects::createBlurShader(SoShaderProgram* prog, bool isHorizontal)
{
    SoVertexShader* vert = new SoVertexShader;
    vert->sourceType = SoVertexShader::GLSL_PROGRAM;
    vert->sourceProgram = TxtVertexShader;
    SoFragmentShader* frag = new SoFragmentShader;
    frag->sourceType = SoFragmentShader::GLSL_PROGRAM;
    frag->sourceProgram = TxtShadowFragmentShader;
    SoShaderParameter1i* texSlot = new SoShaderParameter1i;
    texSlot->name = "u_input_texture";
    texSlot->value = 0;
    SoShaderParameter2f* blurDir = new SoShaderParameter2f;
    blurDir->name = "u_direction";
    if (isHorizontal) {
        blurDir->value.setValue(1.0 / SHADOW_TEX_W, 0);
    }
    else {
        blurDir->value.setValue(0, 1.0 / SHADOW_TEX_W);
    }
    frag->parameter.set1Value(0, texSlot);
    frag->parameter.set1Value(1, blurDir);
    prog->shaderObject.set1Value(0, vert);
    prog->shaderObject.set1Value(1, frag);
}

void SoFC3DEffects::createShadowBlur(SoSceneTexture2* fromTex, SoSceneTexture2* toTex, SoShaderProgram* prog)
{
    static float vertices[4][3] = {
        { -1, -1, 0},
        {  1, -1, 0},
        {  1,  1, 0},
        { -1,  1, 0}
    };

    SoGroup* grp = new SoGroup;
    grp->addChild(prog);
    grp->addChild(fromTex);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.setValues(0, 4, vertices);
    grp->addChild(coords);

    SoFaceSet* faceSet = new SoFaceSet;
    faceSet->numVertices.set1Value(0, 3);  // One quad with 4 vertices
    grp->addChild(faceSet);

    toTex->scene = grp;
    toTex->size.setValue(256, 256);
    toTex->wrapS = SoTexture2::CLAMP;
    toTex->wrapT = SoTexture2::CLAMP;

}

void SoFC3DEffects::createShadowPlane()
{
    static float texCoordVals[4][2] = {
        { 0, 0 },
        { 1, 0 },
        { 1, 1 },
        { 0, 1 }
    };

    ShadowPlaneRoot = new SoGroup;

    // blur shadow pass 1
    ShaderProgHoriz = new SoShaderProgram;
    createBlurShader(ShaderProgHoriz, true);
    BlurPass1Texture = new SoSceneTexture2;
    createShadowBlur(BaseShadowTexture, BlurPass1Texture, ShaderProgHoriz);

    // blur shadow pass 2
    ShaderProgVert = new SoShaderProgram;
    createBlurShader(ShaderProgVert, false);
    BlurPass2Texture = new SoSceneTexture2;
    createShadowBlur(BlurPass1Texture, BlurPass2Texture, ShaderProgVert);

    SoShapeHints* shapeHints = new SoShapeHints;
    shapeHints->shapeType = SoShapeHints::SOLID;
    shapeHints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    ShadowPlaneRoot->addChild(shapeHints);

    SoMaterial* col = new SoMaterial;
    col->transparency.setValue(0.8);
    col->ambientColor.setValue(1.0, 1.0, 1.0);
    //texture->model = SoTexture2::DECAL;

    ShadowPlaneRoot->addChild(col);

    ShadowPlaneCoords = new SoCoordinate3;
    ShadowPlaneRoot->addChild(ShadowPlaneCoords);

    SoTextureCoordinate2* texCoords = new SoTextureCoordinate2;
    texCoords->point.setValues(0, 4, texCoordVals);
    SoTextureCoordinateBinding* texBinding = new SoTextureCoordinateBinding;
    texBinding->value = SoTextureCoordinateBinding::PER_VERTEX;
    SoComplexity* complexity = new SoComplexity;
    complexity->textureQuality = 1.0;
    ShadowPlaneRoot->addChild(complexity);
    ShadowPlaneRoot->addChild(texCoords);
    ShadowPlaneRoot->addChild(texBinding);
    ShadowPlaneRoot->addChild(BlurPass2Texture);

    SoFaceSet* faceSet = new SoFaceSet;
    faceSet->numVertices.set1Value(0, 4);  // One quad with 4 vertices
    ShadowPlaneRoot->addChild(faceSet);
}

void Gui::SoFC3DEffects::createCrossSection()
{

    XSectionRoot = new SoSeparator;
    nullSlicerObject = new SoSeparator;
    nullSlicerObject->ref();
    sliceObject = nullSlicerObject;

    auto drawFillerCallback = new SoCallback;
    drawFillerCallback->setCallback(XSectionDrawFiller, this);
    XSectionRoot->addChild(drawFillerCallback);

    SoMaterial* material = new SoMaterial;
    material->diffuseColor.setValue(0.7f, 0.65f, 0.6f);  // Red color
    XSectionRoot->addChild(material);
    XSectionRoot->addChild(sliceObject);

    auto restoreCallback = new SoCallback;
    restoreCallback->setCallback(XSectionRestoreFunc, this);
    XSectionRoot->addChild(restoreCallback);
}

void Gui::SoFC3DEffects::setSlicerObject(SoSeparator* sliceObj)
{
    if (sliceObj == nullptr) {
        sliceObj = nullSlicerObject;
    }
    XSectionRoot->replaceChild(sliceObject, sliceObj);
    sliceObject = sliceObj;
}

SoFC3DEffects::~SoFC3DEffects()
{
    nullSlicerObject->unref();
    BaseShadowScene->unref();
}

char const TxtVertexShader[] = R"(
#version 330

//layout(location = 0) in vec3 aPos;
const vec2 vertices[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

out vec2 texcoord;

void main()
{
    vec2 aPos = vertices[gl_VertexID];
    gl_Position = vec4(aPos, 0.0, 1.0);

    texcoord = 0.5 * aPos + vec2(0.5);
}
)";

char const TxtShadowFragmentShader[] = R"(
#version 330

uniform sampler2D u_input_texture;
uniform vec2 u_direction;

layout (location = 0) out vec4 out_color;

in vec2 texcoord;

const int M = 16;

// sigma = 10
const float coeffs[M + 1] = float[M + 1](
    0.04425662519949865,
    0.044035873841196206,
    0.043380781642569775,
    0.04231065439216247,
    0.040856643282313365,
    0.039060328279673276,
    0.0369716985390341,
    0.03464682117793548,
    0.03214534135442581,
    0.0295279624870386,
    0.02685404941667096,
    0.02417948052890078,
    0.02155484948872149,
    0.019024086115486723,
    0.016623532195728208,
    0.014381474814203989,
    0.012318109844189502
);

void main()
{
    vec4 sum =  coeffs[0] * texture(u_input_texture, texcoord);
    //float sum = coeffs[0] * texture(u_input_texture, texcoord).a;
    //vec2 dir = vec2(1.0 / 256.0,0);
    vec2 dir = u_direction;

    for (int i = 1; i < M; i += 2)
    {
        float w0 = coeffs[i];
        float w1 = coeffs[i + 1];

        float w = w0 + w1;
        float t = w1 / w;

        sum += w * texture(u_input_texture, texcoord + dir * (float(i) + t));
        sum += w * texture(u_input_texture, texcoord - dir * (float(i) + t));
    }

    out_color = sum;
}
)";

