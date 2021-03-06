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

// Logic includes
#include "vtkSlicerAstroVolumeLogic.h"
#include "vtkSlicerAstroSmoothingLogic.h"
#include "vtkSlicerAstroConfigure.h"

// MRML includes
#include <vtkMRMLAstroVolumeNode.h>
#include <vtkMRMLAstroSmoothingParametersNode.h>

// VTK includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENGL
#include <vtkAstroOpenGLImageBox.h>
#include <vtkAstroOpenGLImageGaussian.h>
#include <vtkAstroOpenGLImageGradient.h>
#endif
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>

// STD includes
#include <cassert>
#include <iostream>

// OpenMP includes
#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
#include <omp.h>
#endif

#ifdef VTK_SLICER_ASTRO_SUPPORT_OPENGL
// vtkOpenGL includes
#include "vtk_glew.h"
#endif

// Qt includes
#include <QtDebug>

#include <iostream>
#include <sys/time.h>

#define UNUSED(expr) (void)(expr)

//----------------------------------------------------------------------------
class vtkSlicerAstroSmoothingLogic::vtkInternal
{
public:
  vtkInternal();
  ~vtkInternal();

  vtkSmartPointer<vtkSlicerAstroVolumeLogic> AstroVolumeLogic;
  vtkSmartPointer<vtkImageData> tempVolumeData;
};

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkInternal::vtkInternal()
{
  this->AstroVolumeLogic = vtkSmartPointer<vtkSlicerAstroVolumeLogic>::New();
  this->tempVolumeData = vtkSmartPointer<vtkImageData>::New();
}

//---------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkInternal::~vtkInternal()
{
}

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerAstroSmoothingLogic);

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::vtkSlicerAstroSmoothingLogic()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerAstroSmoothingLogic::~vtkSlicerAstroSmoothingLogic()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerAstroSmoothingLogic::SetAstroVolumeLogic(vtkSlicerAstroVolumeLogic* logic)
{
  this->Internal->AstroVolumeLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerAstroVolumeLogic* vtkSlicerAstroSmoothingLogic::GetAstroVolumeLogic()
{
  return this->Internal->AstroVolumeLogic;
}

namespace
{
//----------------------------------------------------------------------------
template <typename T> T StringToNumber(const char* num)
{
  std::stringstream ss;
  ss << num;
  T result;
  return ss >> result ? result : 0;
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkSlicerAstroSmoothingLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
  os << indent << "vtkSlicerAstroSmoothingLogic:             " << this->GetClassName() << "\n";
}

//----------------------------------------------------------------------------
void vtkSlicerAstroSmoothingLogic::RegisterNodes()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  vtkMRMLAstroSmoothingParametersNode* pNode = vtkMRMLAstroSmoothingParametersNode::New();
  this->GetMRMLScene()->RegisterNodeClass(pNode);
  pNode->Delete();
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::Apply(vtkMRMLAstroSmoothingParametersNode* pnode,
                                        vtkRenderWindow* renderWindow)
{
  int success = 0;
  switch (pnode->GetFilter())
    {
    case 0:
      {
      if (!(pnode->GetHardware()))
        {
        if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
            fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
          {
          success = this->IsotropicBoxCPUFilter(pnode);
          }
        else
          {
          success = this->AnisotropicBoxCPUFilter(pnode);
          }
        }
      else
        {
        success = this->BoxGPUFilter(pnode, renderWindow);
        }
      break;
      }
    case 1:
      {
        if (!(pnode->GetHardware()))
          {
          if (fabs(pnode->GetParameterX() - pnode->GetParameterY()) < 0.001 &&
              fabs(pnode->GetParameterY() - pnode->GetParameterZ()) < 0.001)
            {
            success = this->IsotropicGaussianCPUFilter(pnode);
            }
          else
            {
            success = this->AnisotropicGaussianCPUFilter(pnode);
            }
          }
        else
          {
          success = this->GaussianGPUFilter(pnode, renderWindow);
          }
      break;
      }
    case 2:
      {
      if (!(pnode->GetHardware()))
        {
        success = this->GradientCPUFilter(pnode);
        }
      else
        {
        success = this->GradientGPUFilter(pnode, renderWindow);
        }
      break;
      }
    }
  return success;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroSmoothing algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter : "
                  "inputVolume not found.");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter : "
                  "outputVolume not found.");
    return 0;
    }

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::AnisotropicBoxCPUFilter : "
                  "imageData with more than one components.");
    return 0.;
    }
  const int numElements = dims[0] * dims[1] * dims[2];
  const int numSlice = dims[0] * dims[1];
  int nItemsX = pnode->GetParameterX();
  if (nItemsX % 2 < 0.001)
    {
    nItemsX++;
    }
  const int Xmax = (int) ((nItemsX - 1) / 2.);
  int nItemsY = pnode->GetParameterY();
  if (nItemsY % 2 < 0.001)
    {
    nItemsY++;
    }
  const int Ymax = (int) ((nItemsY - 1) / 2.);
  int nItemsZ = pnode->GetParameterZ();
  if (nItemsZ % 2 < 0.001)
    {
    nItemsZ++;
    }
  const int Zmax = (int) ((nItemsZ - 1) / 2.);
  const int cont = nItemsX * nItemsY * nItemsZ;
  float *inFPixel = NULL;
  float *outFPixel = NULL;
  double *inDPixel = NULL;
  double *outDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  bool cancel = false;
  int status = 0;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int stat = pnode->GetStatus();

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    if (stat == -1 && omp_get_thread_num() == 0)
    #else
    if (stat == -1)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      {
      cancel = true;
      }
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp flush (cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    if (!cancel)
      {
      switch (DataType)
        {
        case VTK_FLOAT:
          *(outFPixel + elemCnt) = 0.;
          break;
        case VTK_DOUBLE:
          *(outDPixel + elemCnt) = 0.;
          break;
        }

      for (int k = -Zmax; k <= Zmax; k++)
        {
        for (int j = -Ymax; j <= Ymax; j++)
          {
          for (int i = -Xmax; i <= Xmax; i++)
            {
            int posData = elemCnt + i;

            int ref = (int) floor(elemCnt / dims[0]);
            ref *= dims[0];
            if(posData < ref)
              {
              continue;
              }
            if(posData >= ref + dims[0])
              {
              break;
              }

            posData += j * dims[0];
            ref = (int) floor(elemCnt / numSlice);
            ref *= numSlice;
            if(posData < ref)
              {
              continue;
              }
            if(posData >= ref + numSlice)
              {
              break;
              }

            posData += k * numSlice;
            if(posData < 0)
              {
              continue;
              }
            if(posData >= numElements)
              {
              break;
              }

            switch (DataType)
              {
              case VTK_FLOAT:
                *(outFPixel + elemCnt) += *(inFPixel + posData);
                break;
              case VTK_DOUBLE:
                *(outDPixel + elemCnt) += *(inDPixel + posData);
                break;
              }
            }
          }
        }

      switch (DataType)
        {
        case VTK_FLOAT:
          *(outFPixel + elemCnt) /= cont;
          break;
        case VTK_DOUBLE:
          *(outDPixel + elemCnt) /= cont;
          break;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (omp_get_thread_num() == 0)
        {
        if(elemCnt / (numElements / (numProcs * 100)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }
      #else
      if(elemCnt / (numElements / 100) > status)
        {
        status += 10;
        pnode->SetStatus(status);
        }
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      }
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Box Filter (CPU) Kernel Time : "<<mtime<<" ms /n");

  inFPixel = NULL;
  outFPixel = NULL;
  inDPixel = NULL;
  outDPixel = NULL;

  delete inFPixel;
  delete outFPixel;
  delete inDPixel;
  delete outDPixel;

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  return 1;
}


//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::IsotropicBoxCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::IsotropicBoxCPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroSmoothing algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::IsotropicBoxCPUFilter : "
                  "inputVolume not found.");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::IsotropicBoxCPUFilter : "
                  "outputVolume not found.");
    return 0;
    }

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::IsotropicBoxCPUFilter : "
                  "imageData with more than one components.");
    return 0.;
    }
  const int numElements = dims[0] * dims[1] * dims[2];
  const int numSlice = dims[0] * dims[1];
  int nItems = (pnode->GetParameterX());
  if (nItems % 2 < 0.001)
    {
    nItems++;
    }
  const int is = (int) ((nItems - 1) / 2.);

  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  bool cancel = false;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  if (pnode->GetParameterX() > 0.001)
    {
    pnode->SetStatus(10);

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) = 0.;
            break;
          }

        for (int i = -is; i <= is; i++)
          {
          int ii = elemCnt + i;
          int ref = (int) floor(elemCnt / dims[0]);
          ref *= dims[0];
          if(ii < ref)
            {
            continue;
            }
          if(ii >= ref + dims[0])
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) += *(tempFPixel + ii);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii);
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) /= nItems;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) /= nItems;
            break;
          }
        }
      }
    }

  if (cancel)
    {
    outFPixel = NULL;
    tempFPixel = NULL;
    outDPixel = NULL;
    tempDPixel = NULL;

    delete outFPixel;
    delete tempFPixel;
    delete outDPixel;
    delete tempDPixel;

    this->Internal->tempVolumeData->Initialize();

    return 0;
    }

  if (pnode->GetParameterY() > 0.001)
    {
    pnode->SetStatus(40);
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(tempFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(tempDPixel + elemCnt) = 0.;
            break;
          }

        for (int i = -is; i <= is; i++)
          {
          int ii = elemCnt + (i * dims[0]);
          int ref = (int) floor(elemCnt / numSlice);
          ref *= numSlice;
          if(ii < ref)
            {
            continue;
            }
          if(ii >= ref + numSlice)
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(tempFPixel + elemCnt) += *(outFPixel + ii);
              break;
            case VTK_DOUBLE:
              *(tempDPixel + elemCnt) += *(outDPixel + ii);
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(tempFPixel + elemCnt) /= nItems;
            break;
          case VTK_DOUBLE:
            *(tempDPixel + elemCnt) /= nItems;
            break;
          }
        }
      }
    }
  else
    {
    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    this->Internal->tempVolumeData->Modified();
    this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }
    }

  if (cancel)
    {
    outFPixel = NULL;
    tempFPixel = NULL;
    outDPixel = NULL;
    tempDPixel = NULL;

    delete outFPixel;
    delete tempFPixel;
    delete outDPixel;
    delete tempDPixel;

    this->Internal->tempVolumeData->Initialize();

    return 0;
    }

  if (pnode->GetParameterZ() > 0.001)
    {
    pnode->SetStatus(70);
    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) = 0.;
            break;
          }

        for (int i = -is; i <= is; i++)
          {
          int ii = elemCnt + (i * numSlice);
          if(ii < 0)
            {
            continue;
            }
          if(ii >= numElements)
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) += *(tempFPixel + ii);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii);
              break;
            }
          }

        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) /= nItems;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) /= nItems;
            break;
          }
        }
      }
    }
  else
    {
    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Box Filter (CPU) Time : "<<mtime<<" ms /n");

  outFPixel = NULL;
  tempFPixel = NULL;
  outDPixel = NULL;
  tempDPixel = NULL;

  delete outFPixel;
  delete tempFPixel;
  delete outDPixel;
  delete tempDPixel;

  this->Internal->tempVolumeData->Initialize();

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::BoxGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                               vtkRenderWindow* renderWindow)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  UNUSED(renderWindow);
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenGL filtering support.")
  return 0;
  #else

  pnode->SetStatus(1);

  bool cancel = false;

  struct timeval start, end;
  long mtime, seconds, useconds;
  gettimeofday(&start, NULL);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter : "
                  "outputVolume not found.");
    return 0;
    }

  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::BoxGPUFilter : "
                  "imageData with more than one components.");
    return 0;
    }

  vtkNew<vtkAstroOpenGLImageBox> filter;
  filter->SetInputData(outputVolume->GetImageData());
  filter->SetKernelLength(pnode->GetParameterX(),
                          pnode->GetParameterY(),
                          pnode->GetParameterZ());
  filter->SetRenderWindow(renderWindow);

  // check if iterative filters are allowed by the GPU
  const unsigned char* glver = glGetString(GL_VERSION);

  std::string check;
  check = std::string( reinterpret_cast< const char* >(glver));
  std::size_t found = check.find("Mesa");
  if (found!=std::string::npos)
    {
    if (StringToDouble(check.substr(0, 1).c_str()) < 4.)
      {
      vtkWarningMacro("Using Mesa driver with OpenGL version < 4."
                      "The GPU implementation of the isotropic Box filter (3-pass filter using 1-D Kernels) "
                      "is not available with the specifications of the machine in use. "
                      "A 3-D Kernel will be used. ");
      filter->SetIterative(false);
      }
    else
      {
      vtkWarningMacro("Using Mesa driver with OpenGL version >= 4."
                      "The GPU implementation of the isotropic Box filter has not been tested on these specifications, "
                      "please report the success or the failure (punzodavide@hotmail.it).");
      }
    }

#ifdef __APPLE__
  vtkWarningMacro("Using Mac OpenGL version."
                  "The GPU implementation of the isotropic Box filter has not been tested on these specifications, "
                  "please report the success or the failure (punzodavide@hotmail.it).");
#endif

  pnode->SetStatus(20);

  if (pnode->GetStatus() == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    filter->Update();
    }

  pnode->SetStatus(70);

  outputVolume->GetImageData()->DeepCopy(filter->GetOutput());

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro(" Box Filter (GPU, OpenGL) Time : "<<mtime<<" ms /n");

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");


  if(cancel)
    {
    return 0;
    }

  return 1;
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENGL
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroSmoothing algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

   vtkMRMLAstroVolumeNode *inputVolume =
     vtkMRMLAstroVolumeNode::SafeDownCast
       (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
   if (!inputVolume)
     {
     vtkErrorMacro("vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter : "
                   "inputVolume not found.");
     return 0;
     }

   vtkMRMLAstroVolumeNode *outputVolume =
     vtkMRMLAstroVolumeNode::SafeDownCast
       (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
   if (!outputVolume)
     {
     vtkErrorMacro("vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter : "
                   "outputVolume not found.");
     return 0;
     }

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::AnisotropicGaussianCPUFilter : "
                  "imageData with more than one components.");
    return 0.;
    }
  const int numElements = dims[0] * dims[1] * dims[2];
  const int numSlice = dims[0] * dims[1];
  const int Xmax = (int) (pnode->GetKernelLengthX() - 1) / 2.;
  const int Ymax = (int) (pnode->GetKernelLengthY() - 1) / 2.;
  const int Zmax = (int) (pnode->GetKernelLengthZ() - 1) / 2.;
  const int numKernelSlice = pnode->GetKernelLengthX() * pnode->GetKernelLengthY();
  float *inFPixel = NULL;
  float *outFPixel = NULL;
  double *inDPixel = NULL;
  double *outDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      inFPixel = static_cast<float*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      inDPixel = static_cast<double*> (inputVolume->GetImageData()->GetScalarPointer(0,0,0));
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  double *GaussKernel = static_cast<double*> (pnode->GetGaussianKernel3D()->GetVoidPointer(0));

  bool cancel = false;
  int status = 0;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  #pragma omp parallel for schedule(static) shared(pnode, inFPixel, inDPixel, outFPixel, outDPixel, cancel, status)
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
  for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    int stat = pnode->GetStatus();

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    if (stat == -1 && omp_get_thread_num() == 0)
    #else
    if (stat == -1)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      {
      cancel = true;
      }

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp flush (cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    if (!cancel)
      {
      switch (DataType)
        {
        case VTK_FLOAT:
          *(outFPixel + elemCnt) = 0.;
          break;
        case VTK_DOUBLE:
          *(outDPixel + elemCnt) = 0.;
          break;
        }

      for (int k = -Zmax; k <= Zmax; k++)
        {
        for (int j = -Ymax; j <= Ymax; j++)
          {
          for (int i = -Xmax; i <= Xmax; i++)
            {
            int posData = elemCnt + i;
            int ref = (int) floor(elemCnt / dims[0]);
            ref *= dims[0];
            if(posData < ref)
              {
              continue;
              }
            if(posData >= ref + dims[0])
              {
              break;
              }

            posData += j * dims[0];
            ref = (int) floor(elemCnt / numSlice);
            ref *= numSlice;
            if(posData < ref)
              {
              continue;
              }
            if(posData >= ref + numSlice)
              {
              break;
              }

            posData += k * numSlice;
            if(posData < 0)
              {
              continue;
              }
            if(posData >= numElements)
              {
              break;
              }

            int posKernel = (k + Zmax) * numKernelSlice
                          + (j + Ymax) * pnode->GetKernelLengthX() + (i + Xmax);

            switch (DataType)
              {
              case VTK_FLOAT:
                *(outFPixel + elemCnt) += *(inFPixel + posData) * *(GaussKernel + posKernel);
                break;
              case VTK_DOUBLE:
                *(outDPixel + elemCnt) += *(inDPixel + posData) * *(GaussKernel + posKernel);
                break;
              }
            }
          }
        }
      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (omp_get_thread_num() == 0)
        {
        if(elemCnt / (numElements / (numProcs * 100)) > status)
          {
          status += 10;
          pnode->SetStatus(status);
          }
        }
      #else
      if(elemCnt / (numElements / 100) > status)
        {
        status += 10;
        pnode->SetStatus(status);
        }
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      }
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Gaussian Filter (CPU) Time : "<<mtime<<" ms /n");

  inFPixel = NULL;
  outFPixel = NULL;
  inDPixel = NULL;
  outDPixel = NULL;

  delete inFPixel;
  delete outFPixel;
  delete inDPixel;
  delete outDPixel;

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::IsotropicGaussianCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::IsotropicGaussianCPUFilter : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroSmoothing algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  vtkMRMLAstroVolumeNode *inputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
  if (!inputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::IsotropicGaussianCPUFilter : "
                  "inputVolume not found.");
    return 0;
    }

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::IsotropicGaussianCPUFilter : "
                  "outputVolume not found.");
    return 0;
    }

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  const int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::IsotropicGaussianCPUFilter : "
                  "imageData with more than one components.");
    return 0.;
    }
  const int numElements = dims[0] * dims[1] * dims[2];
  const int numSlice = dims[0] * dims[1];
  int is = (int) (pnode->GetKernelLengthX());
  if (is % 2 < 0.001)
    {
    is++;
    }
  const int is2 = (int) - ((is - 1)/ 2);
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }

  double *GaussKernel1D = static_cast<double*> (pnode->GetGaussianKernel1D()->GetVoidPointer(0));
  bool cancel = false;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  if (pnode->GetParameterX() > 0.001)
    {
    pnode->SetStatus(10);

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) = 0.;
            break;
          }

        for (int i = 0; i <= is; i++)
          {
          int ii = elemCnt + i + is2;
          int ref = (int) floor(elemCnt / dims[0]);
          ref *= dims[0];
          if(ii < ref)
            {
            continue;
            }
          if(ii >= ref + dims[0])
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) += *(tempFPixel + ii) * *(GaussKernel1D + i);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii) * *(GaussKernel1D + i);
              break;
            }
          }
        }
      }
    }

  if (cancel)
    {  
    outFPixel = NULL;
    tempFPixel = NULL;
    outDPixel = NULL;
    tempDPixel = NULL;

    delete outFPixel;
    delete tempFPixel;
    delete outDPixel;
    delete tempDPixel;

    this->Internal->tempVolumeData->Initialize();

    return 0;
    }

  if (pnode->GetParameterY() > 0.001)
    {
    pnode->SetStatus(40);

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(tempFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(tempDPixel + elemCnt) = 0.;
            break;
          }
        for (int i = 0; i <= is; i++)
          {
          int ii = elemCnt + ((i + is2) * dims[0]);
          int ref = (int) floor(elemCnt / numSlice);
          ref *= numSlice;
          if(ii < ref)
            {
            continue;
            }
          if(ii >= ref + numSlice)
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(tempFPixel + elemCnt) += *(outFPixel + ii) * *(GaussKernel1D + i);
              break;
            case VTK_DOUBLE:
              *(tempDPixel + elemCnt) += *(outDPixel + ii) * *(GaussKernel1D + i);
              break;
            }
          }
        }
      }
    }
  else
    {
    this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
    this->Internal->tempVolumeData->Modified();
    this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

    switch (DataType)
      {
      case VTK_FLOAT:
        tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }
    }

  if (cancel)
    {  
    outFPixel = NULL;
    tempFPixel = NULL;
    outDPixel = NULL;
    tempDPixel = NULL;

    delete outFPixel;
    delete tempFPixel;
    delete outDPixel;
    delete tempDPixel;

    this->Internal->tempVolumeData->Initialize();

    return 0;
    }

  if (pnode->GetParameterZ() > 0.001)
    {
    pnode->SetStatus(70);

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        switch (DataType)
          {
          case VTK_FLOAT:
            *(outFPixel + elemCnt) = 0.;
            break;
          case VTK_DOUBLE:
            *(outDPixel + elemCnt) = 0.;
            break;
          }
        for (int i = 0; i <= is; i++)
          {
          int ii = elemCnt + ((i + is2) * numSlice);
          if(ii < 0)
            {
            continue;
            }
          if(ii >= numElements)
            {
            break;
            }
          switch (DataType)
            {
            case VTK_FLOAT:
              *(outFPixel + elemCnt) += *(tempFPixel + ii) * *(GaussKernel1D + i);
              break;
            case VTK_DOUBLE:
              *(outDPixel + elemCnt) += *(tempDPixel + ii) * *(GaussKernel1D + i);
              break;
            }
          }
        }
      }
    }
  else
    {
    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);
    }

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Gaussian Filter (CPU) Time : "<<mtime<<" ms /n");

  outFPixel = NULL;
  tempFPixel = NULL;
  outDPixel = NULL;
  tempDPixel = NULL;

  delete outFPixel;
  delete tempFPixel;
  delete outDPixel;
  delete tempDPixel;

  this->Internal->tempVolumeData->Initialize();

  if (cancel)
    {
    return 0;
    }

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GaussianGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                                    vtkRenderWindow *renderWindow)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  UNUSED(renderWindow);
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenGL filtering support.")
  return 0;
  #else

  pnode->SetStatus(1);

  bool cancel = false;

  struct timeval start, end;
  long mtime, seconds, useconds;
  gettimeofday(&start, NULL);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter : "
                  "outputVolume not found.");
    return 0;
    }

  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GaussianGPUFilter : "
                  "imageData with more than one components.");
    return 0;
    }

  vtkNew<vtkAstroOpenGLImageGaussian> filter;
  filter->SetInputData(outputVolume->GetImageData());
  filter->SetKernelLength(pnode->GetKernelLengthX(),
                          pnode->GetKernelLengthY(),
                          pnode->GetKernelLengthZ());
  filter->SetFWHM(pnode->GetParameterX(),
                  pnode->GetParameterY(),
                  pnode->GetParameterZ());
  filter->SetRotationAngles(pnode->GetRx(),
                            pnode->GetRy(),
                            pnode->GetRz());
  filter->SetRenderWindow(renderWindow);

  // check if iterative filters are allowed by the GPU
  const unsigned char* glver = glGetString(GL_VERSION);

  std::string check;
  check = std::string( reinterpret_cast< const char* >(glver));
  std::size_t found = check.find("Mesa");
  if (found!=std::string::npos)
    {
    if (StringToDouble(check.substr(0, 1).c_str()) < 4.)
      {
      vtkWarningMacro("Using Mesa driver with OpenGL version < 4."
                      "The GPU implementation of the isotropic Gaussian filter (3-pass filter using 1-D Kernels) "
                      "is not available with the specifications of the machine in use. "
                      "A 3-D Kernel will be used. ");
      filter->SetIterative(false);
      }
    else
      {
      vtkWarningMacro("Using Mesa driver with OpenGL version >= 4."
                      "The GPU implementation of the isotropic Gaussian filter has not been tested on these specifications, "
                      "please report the success or the failure (punzodavide@hotmail.it).");
      }
    }

#ifdef __APPLE__
  vtkWarningMacro("Using Mac OpenGL version."
                  "The GPU implementation of the Gaussian filter has not been tested on these specifications, "
                  "please report the success or the failure (punzodavide@hotmail.it).");
#endif

  pnode->SetStatus(20);

  if (pnode->GetStatus() == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    filter->Update();
    }

  pnode->SetStatus(70);

  outputVolume->GetImageData()->DeepCopy(filter->GetOutput());

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Gaussian Filter (GPU, OpenGL) Time : "<<mtime<<" ms /n");

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);
  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro("Update : "<<mtime<<" ms /n");

  if (cancel)
    {
    return 0;
    }

  return 1;
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENGL
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GradientCPUFilter(vtkMRMLAstroSmoothingParametersNode* pnode)
{
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::GradientCPUFilter : "
                  "this release of SlicerAstro has been built "
                  "without OpenMP support. It may results that "
                  "the AstroSmoothing algorithm will show poor performance.")
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

   vtkMRMLAstroVolumeNode *inputVolume =
     vtkMRMLAstroVolumeNode::SafeDownCast
       (this->GetMRMLScene()->GetNodeByID(pnode->GetInputVolumeNodeID()));
   if (!inputVolume)
     {
     vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientCPUFilter : "
                   "inputVolume not found.");
     return 0;
     }

   vtkMRMLAstroVolumeNode *outputVolume =
     vtkMRMLAstroVolumeNode::SafeDownCast
       (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));
   if (!outputVolume)
     {
     vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientCPUFilter : "
                   "outputVolume not found.");
     return 0;
     }

  this->Internal->tempVolumeData->Initialize();
  this->Internal->tempVolumeData->DeepCopy(outputVolume->GetImageData());
  this->Internal->tempVolumeData->Modified();
  this->Internal->tempVolumeData->GetPointData()->GetScalars()->Modified();

  int *dims = outputVolume->GetImageData()->GetDimensions();
  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientCPUFilter : "
                  "imageData with more than one components.");
    return 0.;
    }
  const int numElements = dims[0] * dims[1] * dims[2];
  const int numSlice = dims[0] * dims[1];
  const double noise = StringToDouble(outputVolume->GetAttribute("SlicerAstro.RMS"));
  const double noise2 = noise * noise * pnode->GetK() * pnode->GetK();
  float *outFPixel = NULL;
  float *tempFPixel = NULL;
  double *outDPixel = NULL;
  double *tempDPixel = NULL;
  const int DataType = outputVolume->GetImageData()->GetPointData()->GetScalars()->GetDataType();
  switch (DataType)
    {
    case VTK_FLOAT:
      outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempFPixel = static_cast<float*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    case VTK_DOUBLE:
      outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
      tempDPixel = static_cast<double*> (this->Internal->tempVolumeData->GetScalarPointer(0,0,0));
      break;
    default:
      vtkErrorMacro("Attempt to allocate scalars of type not allowed");
      return 0;
    }
  bool cancel = false;

  #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
  int numProcs = 0;
  if (pnode->GetCores() == 0)
    {
    numProcs = omp_get_num_procs();
    }
  else
    {
    numProcs = pnode->GetCores();
    }

  omp_set_num_threads(numProcs);
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP

  struct timeval start, end;

  long mtime, seconds, useconds;

  gettimeofday(&start, NULL);

  pnode->SetStatus(1);

  for (int i = 1; i <= pnode->GetAccuracy(); i++)
    {

    #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
    #pragma omp parallel for schedule(static) shared(pnode, outFPixel, outDPixel, tempFPixel, tempDPixel, cancel)
    #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
    for (int elemCnt = 0; elemCnt < numElements; elemCnt++)
      {
      int status = pnode->GetStatus();

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (status == -1 && omp_get_thread_num() == 0)
      #else
      if (status == -1)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
        {
        cancel = true;
        }

      #ifdef VTK_SLICER_ASTRO_SUPPORT_OPENMP
      #pragma omp flush (cancel)
      #endif // VTK_SLICER_ASTRO_SUPPORT_OPENMP
      if (!cancel)
        {
        int x1 = elemCnt - 1;
        int ref = (int) floor(elemCnt / dims[0]);
        ref *= dims[0];
        if(x1 < ref)
          {
          x1++;
          }
        int x2 = elemCnt + 1;
        if(x2 >= ref + dims[0])
          {
          x2--;
          }

        int y1 = elemCnt - dims[0];
        ref = (int) floor(elemCnt / numSlice);
        ref *= numSlice;
        if(y1 < ref)
          {
          y1 += dims[0];
          }
        int y2 = elemCnt + dims[0];
        if(y2 >= ref + numSlice)
          {
          y2 -= dims[0];
          }

        int z1 = elemCnt - numSlice;
        if(z1 < 0)
          {
          z1 += numSlice;
          }
        int z2 = elemCnt + numSlice;
        if(z2 >= numElements)
          {
          z2 -= numSlice;
          }

        double Pixel2, norm;
        double cX, cY, cZ;

        switch (DataType)
          {
          case VTK_FLOAT:
            Pixel2 = *(outFPixel + elemCnt) * *(outFPixel + elemCnt);
            norm = 1. + (Pixel2 / noise2);
            cX = ((*(outFPixel + x1) - *(outFPixel + elemCnt)) +
                  (*(outFPixel + x2) - *(outFPixel + elemCnt))) * pnode->GetParameterX();
            cY = ((*(outFPixel + y1) - *(outFPixel + elemCnt)) +
                  (*(outFPixel + y2) - *(outFPixel + elemCnt))) * pnode->GetParameterY();
            cZ = ((*(outFPixel + z1) - *(outFPixel + elemCnt)) +
                  (*(outFPixel + z2) - *(outFPixel + elemCnt))) * pnode->GetParameterZ();

            *(tempFPixel + elemCnt) = *(outFPixel + elemCnt) +
                                      pnode->GetTimeStep() * (cX + cY + cZ) / norm;
            break;
          case VTK_DOUBLE:
            Pixel2 = *(outDPixel + elemCnt) * *(outDPixel + elemCnt);
            norm = 1. + (Pixel2 / noise2);
            cX = ((*(outDPixel + x1) - *(outDPixel + elemCnt)) +
                  (*(outDPixel + x2) - *(outDPixel + elemCnt))) * pnode->GetParameterX();
            cY = ((*(outDPixel + y1) - *(outDPixel + elemCnt)) +
                  (*(outDPixel + y2) - *(outDPixel + elemCnt))) * pnode->GetParameterY();
            cZ = ((*(outDPixel + z1) - *(outDPixel + elemCnt)) +
                  (*(outDPixel + z2) - *(outDPixel + elemCnt))) * pnode->GetParameterZ();

            *(tempDPixel + elemCnt) = *(outDPixel + elemCnt) +
                                      pnode->GetTimeStep() * (cX + cY + cZ) / norm;
            break;
          }
        }
      }

    if (cancel)
      {  
      outFPixel = NULL;
      tempFPixel = NULL;
      outDPixel = NULL;
      tempDPixel = NULL;

      delete outFPixel;
      delete tempFPixel;
      delete outDPixel;
      delete tempDPixel;

      this->Internal->tempVolumeData->Initialize();

      return 0;
      }

    outputVolume->GetImageData()->DeepCopy(this->Internal->tempVolumeData);

    switch (DataType)
      {
      case VTK_FLOAT:
        outFPixel = static_cast<float*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      case VTK_DOUBLE:
        outDPixel = static_cast<double*> (outputVolume->GetImageData()->GetScalarPointer(0,0,0));
        break;
      default:
        vtkErrorMacro("Attempt to allocate scalars of type not allowed");
        return 0;
      }

    pnode->SetStatus((int) i * 100 / pnode->GetAccuracy());
    }


  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Intensity driven Gradient Filter (CPU) Time : "<<mtime<<" ms /n");

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  double noiseMean = StringToDouble(outputVolume->GetAttribute("SlicerAstro.RMSMEAN"));

  for( int elemCnt = 0; elemCnt < numElements; elemCnt++)
    {
    switch (DataType)
      {
      case VTK_FLOAT:
        *(outFPixel+elemCnt) -= noiseMean;
        break;
      case VTK_DOUBLE:
        *(outDPixel+elemCnt) -= noiseMean;
        break;
      }
    }

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  outFPixel = NULL;
  tempFPixel = NULL;
  outDPixel = NULL;
  tempDPixel = NULL;

  delete outFPixel;
  delete tempFPixel;
  delete outDPixel;
  delete tempDPixel;

  this->Internal->tempVolumeData->Initialize();

  return 1;
}

//----------------------------------------------------------------------------
int vtkSlicerAstroSmoothingLogic::GradientGPUFilter(vtkMRMLAstroSmoothingParametersNode *pnode,
                                                    vtkRenderWindow* renderWindow)
{  
  #ifndef VTK_SLICER_ASTRO_SUPPORT_OPENGL
  UNUSED(renderWindow);
  vtkWarningMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter "
                  "this release of SlicerAstro has been built "
                  "without OpenGL filtering support.")
  return 0;
  #else

  // check if iterative filters are allowed by the GPU
  const unsigned char* glver = glGetString(GL_VERSION);

  std::string check;
  check = std::string( reinterpret_cast< const char* >(glver));
  std::size_t found = check.find("Mesa");
  if (found!=std::string::npos)
    {
    if (StringToDouble(check.substr(0, 1).c_str()) < 4.)
      {
      vtkWarningMacro("Using Mesa driver with OpenGL version < 4."
                      "The GPU implementation of the Intensity-Driven Gradient filter "
                      "is not available with the specifications of the machine in use.");
      return 0;
      }
    else
      {
      vtkWarningMacro("Using Mesa driver with OpenGL version >= 4."
                      "The GPU implementation of the Intensity-Driven Gradient filter has not been tested on these specifications, "
                      "please report the success or the failure (punzodavide@hotmail.it).");
      }
    }

#ifdef __APPLE__
  vtkWarningMacro("Using Mac OpenGL version."
                  "The GPU implementation of the Intensity-Driven Gradient filter has not been tested on these specifications, "
                  "please report the success or the failure (punzodavide@hotmail.it).");
#endif

  pnode->SetStatus(1);

  bool cancel = false;

  struct timeval start, end;
  long mtime, seconds, useconds;
  gettimeofday(&start, NULL);

  vtkMRMLAstroVolumeNode *outputVolume =
    vtkMRMLAstroVolumeNode::SafeDownCast
      (this->GetMRMLScene()->GetNodeByID(pnode->GetOutputVolumeNodeID()));

  if (!outputVolume)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter : "
                  "outputVolume not found.");
    return 0;
    }

  const int numComponents = outputVolume->GetImageData()->GetNumberOfScalarComponents();
  if (numComponents > 1)
    {
    vtkErrorMacro("vtkSlicerAstroSmoothingLogic::GradientGPUFilter : "
                  "imageData with more than one components.");
    return 0;
    }

  vtkNew<vtkAstroOpenGLImageGradient> filter;
  filter->SetInputData(outputVolume->GetImageData());
  filter->SetCl(pnode->GetParameterX(),
                pnode->GetParameterY(),
                pnode->GetParameterZ());
  filter->SetK(pnode->GetK());
  filter->SetAccuracy(pnode->GetAccuracy());
  filter->SetTimeStep(pnode->GetTimeStep());
  filter->SetRMS(StringToDouble(outputVolume->GetAttribute("SlicerAstro.RMS")));

  filter->SetRenderWindow(renderWindow);

  pnode->SetStatus(20);

  if (pnode->GetStatus() == -1)
    {
    cancel = true;
    }

  if(!cancel)
    {
    filter->Update();
    }

  pnode->SetStatus(70);

  outputVolume->GetImageData()->DeepCopy(filter->GetOutput());

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
  vtkDebugMacro(" Intensity-Driven Gradient Filter (GPU, OpenGL) Time : "<<mtime<<" ms /n");

  gettimeofday(&start, NULL);

  outputVolume->UpdateRangeAttributes();
  outputVolume->UpdateNoiseAttributes();

  pnode->SetStatus(100);

  gettimeofday(&end, NULL);

  seconds  = end.tv_sec  - start.tv_sec;
  useconds = end.tv_usec - start.tv_usec;

  mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

  vtkDebugMacro("Update Time : "<<mtime<<" ms /n");

  if(cancel)
    {
    return 0;
    }

  return 1;
  #endif // VTK_SLICER_ASTRO_SUPPORT_OPENGL
}
