/*==============================================================================

  Copyright (c) Kapteyn Astronomical Institute
  University of Groningen, Groningen, Netherlands. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Davide Punzo, Kapteyn Astronomical Institute,
  and was supported through the European Research Council grant nr. 291531.

==============================================================================*/

#ifndef __qSlicerAstroSmoothingModuleWidget_h
#define __qSlicerAstroSmoothingModuleWidget_h

// SlicerQt includes
#include "qSlicerAbstractModuleWidget.h"

#include "qSlicerAstroSmoothingModuleExport.h"

class qSlicerAstroSmoothingModuleWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLAstroSmoothingParametersNode;

/// \ingroup Slicer_QtModules_AstroSmoothing
class Q_SLICER_QTMODULES_ASTROSMOOTHING_EXPORT qSlicerAstroSmoothingModuleWidget :
  public qSlicerAbstractModuleWidget
{
  Q_OBJECT

public:

  typedef qSlicerAbstractModuleWidget Superclass;
  qSlicerAstroSmoothingModuleWidget(QWidget *parent=0);
  virtual ~qSlicerAstroSmoothingModuleWidget();

  /// Get vtkMRMLAstroSmoothingParametersNode
  Q_INVOKABLE vtkMRMLAstroSmoothingParametersNode* mrmlAstroSmoothingParametersNode()const;

public slots:
  void onApply();

protected:
  QScopedPointer<qSlicerAstroSmoothingModuleWidgetPrivate> d_ptr;

  virtual void setMRMLScene(vtkMRMLScene*);
  void initializeParameterNode(vtkMRMLScene*);

protected slots:
  void onAccuracyChanged(double value);
  void onAutoRunChanged(bool value);
  void onComputationCancelled();
  void onComputationFinished();
  void onComputationStarted();
  void onCurrentFilterChanged(int index);
  void onEndCloseEvent();
  void onHardwareChanged(int index);
  void onInputVolumeChanged(vtkMRMLNode*);
  void onKChanged(double value);
  void onLinkChanged(bool value);
  void onMasksCommandChanged();
  void onMRMLAstroSmoothingParametersNodeModified();
  void onMRMLSelectionNodeModified(vtkObject* sender);
  void onModeChanged();
  void onOutputVolumeChanged(vtkMRMLNode*);
  void onParameterXChanged(double value);
  void onParameterYChanged(double value);
  void onParameterZChanged(double value);
  void onRxChanged(double value);
  void onRyChanged(double value);
  void onRzChanged(double value);
  void onSegmentEditorNodeModified(vtkObject* sender);
  void onTimeStepChanged(double value);
  void updateProgress(int value);
  void setMRMLAstroSmoothingParametersNode(vtkMRMLNode*);

private:
  Q_DECLARE_PRIVATE(qSlicerAstroSmoothingModuleWidget);
  Q_DISABLE_COPY(qSlicerAstroSmoothingModuleWidget);
};

#endif
