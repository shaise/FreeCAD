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

#ifndef GUI_SOFC3DEFFECTSSETUP_H
#define GUI_SOFC3DEFFECTSSETUP_H

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFString.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <string>

namespace Gui
{

class View3DInventorViewer;

class SoFC3DEffectsSetup : public SoSeparator
{
    using inherited = SoSeparator;
    SO_NODE_HEADER(Gui::SoFC3DEffectsSetup);

public:
    static void initClass();
    //static void finish();
    SoFC3DEffectsSetup();

    SoSFString documentName;
    SoSFString objectName;
    SoSFString subElementName;
    SoSFBool enableAmbientOclusion;
    SoSFBool enableCrossSections;
    SoSFBool enableShadows;

    void setScene(SoNode* scene);
    void setRenderManager(SoRenderManager* renderManager);
    void SetViewer(View3DInventorViewer* viewer);
    void HideLines();
    void RestoreLinesState();

    //void handleEvent(SoHandleEventAction* action) override;
    //void GLRenderBelowPath(SoGLRenderAction* action) override;
    //void GLRenderInPath(SoGLRenderAction* action) override;
    //static  void turnOffCurrentHighlight(SoGLRenderAction* action);
    //void GLRender(SoGLRenderAction* action) override;
    void updateGeometry();
    SoSeparator* getSlicerObject();

protected:

    void createScene();
    void createSlicerObject();

    // shadow generation
    bool updateBoundingBox();
    void updateXSectionCoords();

    // cross section generation
    void createCrossSectionSetup();

    ~SoFC3DEffectsSetup() override;

    SoNode* Scene;
    SoNode* nullScene;
    SoRenderManager* RenderManager;
    View3DInventorViewer* Viewer3D;
    SoSwitch* SSAOSwitch;
    SoSwitch* XSectionSwitch;
    SbBox3f BoundingBox;
    SbBox3f OldBoundingBox;
    std::string saveRenderMode;

    // cross-section nodes
    SoTexture2* XSectionTexture;
    SoCoordinate3* XSectionCoords;
    SoSeparator* XSectionRoot;
    SoSeparator* sliceObject;
};

}

#endif  // GUI_SOFC3DEFFECTSSETUP_H
