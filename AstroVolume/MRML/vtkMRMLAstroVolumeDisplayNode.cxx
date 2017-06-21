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
  and was supported through the European Research Consil grant nr. 291531.

==============================================================================*/

#include <math.h>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>

// MRML includes
#include <vtkMRMLAstroVolumeDisplayNode.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLUnitNode.h>
#include <vtkMRMLVolumeNode.h>

// VTK includes
#include <vtkAlgorithm.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkStringArray.h>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLAstroVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::vtkMRMLAstroVolumeDisplayNode()
{
  this->SpaceQuantities = vtkStringArray::New();
  this->SpaceQuantities->SetName("Tokens");
  this->SpaceQuantities->SetNumberOfValues(3);
  this->SpaceQuantities->SetValue(0, "time");
  this->SpaceQuantities->SetValue(1, "length");
  this->SpaceQuantities->SetValue(2, "velocity");
  this->Space = 0;
  this->SetSpace("WCS");
  this->WCSStatus = 0;
  this->WCS = new struct wcsprm;
  this->WCS->flag=-1;
  wcserr_enable(1);
  if((this->WCSStatus = wcsini(1,0,this->WCS)))
    {
    vtkErrorMacro("wcsini ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
    }
}

//----------------------------------------------------------------------------
vtkMRMLAstroVolumeDisplayNode::~vtkMRMLAstroVolumeDisplayNode()
{

  if (this->SpaceQuantities)
    {
    this->SpaceQuantities->Delete();
    }

  if (this->Space)
    {
    delete [] this->Space;
    }

  if(this->WCS)
    {
    if((this->WCSStatus = wcsfree(this->WCS)))
      {
      vtkErrorMacro("wcsfree ERROR "<<WCSStatus<<":\n"<<
                      "Message from "<<WCS->err->function<<
                      "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                      ": \n"<<WCS->err->msg<<"\n");
      }
    delete [] this->WCS;
    this->WCS = NULL;
    }
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
int StringToInt(const char* str)
{
  return StringToNumber<int>(str);
}

//----------------------------------------------------------------------------
double StringToDouble(const char* str)
{
  return StringToNumber<double>(str);
}

//----------------------------------------------------------------------------
template <typename T> std::string NumberToString(T V)
{
  std::string stringValue;
  std::stringstream strstream;
  strstream << V;
  strstream >> stringValue;
  return stringValue;
}

//----------------------------------------------------------------------------
std::string IntToString(int Value)
{
  return NumberToString<int>(Value);
}

//----------------------------------------------------------------------------
std::string DoubleToString(double Value)
{
  return NumberToString<double>(Value);
}

}// end namespace

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkIndent indent(nIndent);

  std::string quantities = "";
  int i,j,k;

  if (this->SpaceQuantities)
    {
    for(i = 0; i < this->SpaceQuantities->GetNumberOfValues(); i++)
      {
      quantities +=  this->SpaceQuantities->GetValue(i) + ";";
      }
    }

  of << indent << " SpaceQuantities=\"" << quantities << "\"";
  of << indent << " Space=\"" << (this->Space ? this->Space : "") << "\"";

  std::string pre = " SlicerAstro.WCS.";
  std::string und = "UNDEFINED";

  of << indent << pre << "flag=\"" << this->WCS->flag << "\"";
  of << indent << pre << "naxis=\"" << this->WCS->naxis << "\"";
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "crpix"<<i<<"=\"" << this->WCS->crpix[i] << "\"";
    }

  // Linear transformation.
  k = 0;
  for (i = 0; i < this->WCS->naxis; i++)
    {
    for (j = 0; j < this->WCS->naxis; j++)
      {
      of << indent << pre << "pc"<<k<<"=\"" << this->WCS->pc[k] << "\"";
      k++;
      }
    }

  // Coordinate increment at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "cdelt"<<i<<"=\"" << this->WCS->cdelt[i] << "\"";
    }

  // Coordinate value at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "crval"<<i<<"=\"" << this->WCS->crval[i] << "\"";
    }

  // Coordinate units and type.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "cunit"<<i<<"=\"" << this->WCS->cunit[i] << "\"";
    }

  for (i = 0; i < this->WCS->naxis; i++)
    {
    of << indent << pre << "ctype"<<i<<"=\"" << this->WCS->ctype[i] << "\"";
    }

  // Celestial and spectral transformation parameters.
  of << indent << pre << "lonpole=\"" << this->WCS->lonpole << "\"";
  of << indent << pre << "latpole=\"" << this->WCS->latpole << "\"";
  of << indent << pre << "restfrq=\"" << this->WCS->restfrq << "\"";
  of << indent << pre << "restwav=\"" << this->WCS->restwav << "\"";
  of << indent << pre << "npv=\"" << this->WCS->npv << "\"";
  of << indent << pre << "npvmax=\"" << this->WCS->npvmax << "\"";

  for (i = 0; i < this->WCS->npv; i++)
    {
    of << indent << pre << "pvi"<<i<<"=\"" <<  (this->WCS->pv[i]).i << "\"";
    of << indent << pre << "pvvalue"<<i<<"=\"" <<  (this->WCS->pv[i]).value << "\"";
    }

  of << indent << pre << "nps=\"" << this->WCS->nps << "\"";
  of << indent << pre << "npsmax=\"" << this->WCS->npsmax << "\"";

  for (i = 0; i < this->WCS->nps; i++)
    {
    of << indent << pre << "psi"<<i<<"=\"" <<  (this->WCS->ps[i]).i << "\"";
    of << indent << pre << "psvalue"<<i<<"=\"" <<  (this->WCS->ps[i]).value << "\"";
    }

  // Alternate linear transformations.
  k = 0;
  if (this->WCS->cd)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        of << indent << pre << "cd"<<k<<"=\"" << this->WCS->cd[k] << "\"";
        k++;
        }
      }
    }

  if (this->WCS->crota)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      of << indent << pre << "crota"<<i<<"=\"" << this->WCS->crota[i] << "\"";
      }
    }

  of << indent << pre << "altlin=\"" << this->WCS->altlin << "\"";
  of << indent << pre << "velref=\"" << this->WCS->velref << "\"";
  of << indent << pre << "alt=\"" << this->WCS->alt << "\"";
  of << indent << pre << "colnum=\"" << this->WCS->colnum << "\"";

  if  (this->WCS->colax)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      of << indent << pre << "colax"<<i<<"=\"" << this->WCS->colax[i] << "\"";
      }
    }

  if (this->WCS->wcsname[0] == '\0')
    {
    of << indent << pre << "wcsname=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "wcsname=\"" << this->WCS->wcsname << "\"";
    }

  if (this->WCS->cname)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if  (this->WCS->cname[i][0] == '\0')
        {
        of << indent << pre << "cname"<<i<<"=\"" << und << "\"";
        }
      else
        {
        of << indent << pre << "cname"<<i<<"=\"" << this->WCS->cname[i] << "\"";
        }
      }
    }

  if (this->WCS->crder)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined (this->WCS->crder[i]))
        {
        of << indent << pre << "crder"<<i<<"=\"" << und << "\"";
        }
      else
        {
        of << indent << pre << "crder"<<i<<"=\"" << this->WCS->crder[i] << "\"";
        }
      }
    }

  if (this->WCS->csyer)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined (this->WCS->csyer[i]))
        {
        of << indent << pre << "csyer"<<i<<"=\"" << und << "\"";
        }
      else
        {
        of << indent << pre << "csyer"<<i<<"=\"" << this->WCS->csyer[i] << "\"";
        }
      }
    }

  if (this->WCS->radesys[0] == '\0')
    {
    of << indent << pre << "radesys=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "radesys=\"" << this->WCS->radesys << "\"";
    }

  if (undefined (this->WCS->equinox))
    {
    of << indent << pre << "equinox=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "equinox=\"" << this->WCS->equinox << "\"";
    }

  if (this->WCS->specsys[0] == '\0')
    {
    of << indent << pre << "specsys=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "specsys=\"" << this->WCS->specsys << "\"";
    }

  if (this->WCS->ssysobs[0] == '\0')
    {
    of << indent << pre << "ssysobs=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "ssysobs=\"" << this->WCS->ssysobs << "\"";
    }

  if (undefined (this->WCS->velosys))
    {
    of << indent << pre << "velosys=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "velosys=\"" << this->WCS->velosys << "\"";
    }

  if (this->WCS->ssyssrc[0] == '\0')
    {
    of << indent << pre << "ssyssrc=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "ssyssrc=\"" << this->WCS->ssyssrc << "\"";
    }

  if (undefined (this->WCS->zsource))
    {
    of << indent << pre << "zsource=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "zsource=\"" << this->WCS->zsource << "\"";
    }

  for (i = 0; i < 3; i++)
    {
    if (undefined (this->WCS->obsgeo[i]))
      {
      of << indent << pre << "obsgeo"<<i<<"=\"" << und << "\"";
      }
    else
      {
      of << indent << pre << "obsgeo"<<i<<"=\"" << this->WCS->obsgeo[i] << "\"";
      }
    }

  if (this->WCS->dateobs[0] == '\0')
    {
    of << indent << pre << "dateobs=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "dateobs=\"" << this->WCS->dateobs << "\"";
    }

  if (this->WCS->dateavg[0] == '\0')
    {
    of << indent << pre << "dateavg=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "dateavg=\"" << this->WCS->dateavg << "\"";
    }

  if (undefined (this->WCS->mjdobs))
    {
    of << indent << pre << "mjdobs=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "mjdobs=\"" << this->WCS->mjdobs << "\"";
    }

  if (undefined (this->WCS->mjdavg))
    {
    of << indent << pre << "mjdavg=\"" << und << "\"";
    }
  else
    {
    of << indent << pre << "mjdavg=\"" << this->WCS->mjdavg << "\"";
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::SetWCSStruct(struct wcsprm* wcstemp)
{

  if(!wcstemp)
    {
    vtkErrorMacro("wcsprm is invalid!");
    return;
    }

  this->WCS->flag=-1;
  if ((this->WCSStatus = wcscopy(1, wcstemp, this->WCS)))
    {
    vtkErrorMacro("wcscopy ERROR "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    }
  if ((this->WCSStatus = wcsset (this->WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<WCSStatus<<":\n"<<
                  "Message from "<<WCS->err->function<<
                  "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                  ": \n"<<WCS->err->msg<<"\n");
    }
}

//----------------------------------------------------------------------------
wcsprm *vtkMRMLAstroVolumeDisplayNode::GetWCSStruct()
{
  return WCS;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::GetReferenceSpace(const double ijk[3],
                                                      double SpaceCoordinates[3])
{
  if (this->Space != NULL)
    {
    if (!strcmp(Space, "WCS"))
      {
      double phi[1], imgcrd[4], theta[1], ijkm [] = {0., 0., 0., 0.}, SpaceCoordinatesM [] = {0., 0., 0., 0.};
      int stati[1];

      std::copy(ijk, ijk + 3, ijkm);

      if ((this->WCSStatus = wcsp2s(this->WCS, 1, 4, ijkm, imgcrd, phi, theta, SpaceCoordinatesM, stati)))
        {
        vtkErrorMacro("wcsp2s ERROR "<<WCSStatus<<":\n"<<
                        "Message from "<<WCS->err->function<<
                        "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                        ": \n"<<WCS->err->msg<<"\n");
        }

      std::copy(SpaceCoordinatesM, SpaceCoordinatesM + 3, SpaceCoordinates);
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::GetIJKSpace(const double SpaceCoordinates[3],
                                                double ijk[3])
{
  if (this->Space != NULL)
    {
    if (!strcmp(Space, "WCS"))
      {
      double phi[1], imgcrd[4], theta[1], ijkm [] = {0., 0., 0., 0.}, SpaceCoordinatesM [] = {0., 0., 0., 0.};
      int stati[1];

      std::copy(SpaceCoordinates, SpaceCoordinates + 3, SpaceCoordinatesM);

      if ((this->WCSStatus = wcss2p(this->WCS, 1, 4, SpaceCoordinatesM, phi, theta, imgcrd, ijkm, stati)))
        {
        vtkErrorMacro("wcss2p ERROR "<<WCSStatus<<":\n"<<
                        "Message from "<<WCS->err->function<<
                        "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                        ": \n"<<WCS->err->msg<<"\n");
        }
      std::copy(ijkm, ijkm + 3, ijk);
      }
    }
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::GetIJKSpace(std::vector<double> SpaceCoordinates,
                                                double ijk[3])
{
  if (this->Space != NULL)
    {
    if (!strcmp(Space, "WCS"))
      {
      double phi[1], imgcrd[4], theta[1], ijkm [] = {0., 0., 0., 0.}, SpaceCoordinatesM [] = {0., 0., 0., 0.};
      int stati[1];

      SpaceCoordinatesM[0] = SpaceCoordinates[0];
      SpaceCoordinatesM[1] = SpaceCoordinates[1];
      SpaceCoordinatesM[2] = SpaceCoordinates[2];

      if ((this->WCSStatus = wcss2p(this->WCS, 1, 4, SpaceCoordinatesM, phi, theta, imgcrd, ijkm, stati)))
        {
        vtkErrorMacro("wcss2p ERROR "<<WCSStatus<<":\n"<<
                        "Message from "<<WCS->err->function<<
                        "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                        ": \n"<<WCS->err->msg<<"\n");
        }
      std::copy(ijkm, ijkm + 3, ijk);
      }
    }
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(const double worldA,
                                                          const double worldB,
                                                          const double wcsStep,
                                                          vtkMRMLUnitNode *node)
{
  if(!node)
    {
    return 0.;
    }

  double temp;

  if(worldA < worldB)
    {
    temp = worldA;
    }
  else
    {
    temp = worldB;
    }

  return temp - fmod(temp, wcsStep) - wcsStep;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxisX(const double worldA,
                                                           const double worldB,
                                                           const double wcsStep)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
    return vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(worldA, worldB, wcsStep, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxisY(const double worldA,
                                                           const double worldB,
                                                           const double wcsStep)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
    return vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(worldA, worldB, wcsStep, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxisZ(const double worldA,
                                                           const double worldB,
                                                           const double wcsStep)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
    return vtkMRMLAstroVolumeDisplayNode::GetFirstWcsTickAxis(worldA, worldB, wcsStep, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(const double wcsLength,
                                                          int* numberOfPoints,
                                                          vtkMRMLUnitNode *node)
{
  if(!node)
    {
    return 0.;
    }

  int nPoint = 1000;
  double step = wcsLength;
  int s = 1;

  if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    step *= 0.066666666666667;
    }

  while (nPoint > numberOfPoints[0])
    {
    step = step * s / 5.;
    std::string displayValueString;
    std::stringstream strstream;

    strstream.precision(1);
    strstream << step;
    strstream >> displayValueString;
    for (int i = 9; i > 0; i--)
      {

      std::size_t found = displayValueString.find(IntToString(i));
      std::size_t foundE = displayValueString.find("e");

      if (found != std::string::npos && found > foundE)
        {
        continue;
        }

      if (found != std::string::npos)
        {
        if((!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
            !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
           && step < 0.6 && step > 0.095)
          {
          if(i > 6)
            {
            displayValueString.replace(found, found+1, "10");
            }
          else if(i > 3)
            {
            displayValueString.replace(found, found+1, "5");
            }
          else if(i > 1)
            {
            displayValueString.replace(found, found+1, "25");
            }
          else
            {
            displayValueString.replace(found, found+1, "08333333333333333333333");
            }
          }
        else if((!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
                 !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
                && step < 0.095 && step > 0.0045)
          {
          if(i > 4)
            {
            displayValueString.replace(found, found+1, "8333333333333333333333");
            }
          else if(i >= 2)
            {
            displayValueString.replace(found, found+1, "3333333333333333333333");
            }
          else
            {
            displayValueString.replace(found, found+1, "1666666666666666666666");
            }
          }
        else if((!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
                 !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
                && step < 0.0045 && step > 0.001)
          {
          if(i > 5)
            {
            displayValueString.replace(found, found+1, "8333333333333333333333");
            }
          else if(i >= 3)
            {
            displayValueString.replace(found, found+1, "4166666666666666666666");
            }
          else
            {
            displayValueString.replace(found, found+1, "1388888888888888888888");
            }
          }
        else if((!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
                 !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
                && step < 0.001)
          {
          if(i > 6)
            {
            displayValueString.replace(found, found+1, "8333333333333334");
            }
          else if(i > 3)
            {
            displayValueString.replace(found, found+1, "5555555555555556");
            }
          else
            {
            displayValueString.replace(found, found+1, "2777777777777778");
            }
          }
        else
          {
          if(i > 6)
            {
            displayValueString.replace(found, found+1, "10");
            }
          else if(i > 3)
            {
            displayValueString.replace(found, found+1, "5");
            }
          else if (i>= 2)
            {
            displayValueString.replace(found, found+1, "2");
            }
          else
            {
            displayValueString.replace(found, found+1, "1");
            }
          }
        break;
        }
      }
    step = StringToDouble(displayValueString.c_str());
    if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
      {
      step /= 0.066666666666667;
      }
    nPoint = (int) (wcsLength / step);
    s *= 2;
    }
  *numberOfPoints = nPoint + 3;
  return step;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxisX(const double wcsLength,
                                                          int* numberOfPoints)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
    return vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(wcsLength, numberOfPoints, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxisY(const double wcsLength,
                                                          int* numberOfPoints)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
    return vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(wcsLength, numberOfPoints, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
double vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxisZ(const double wcsLength,
                                                          int* numberOfPoints)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
    return vtkMRMLAstroVolumeDisplayNode::GetWcsTickStepAxis(wcsLength, numberOfPoints, unitNode);
    }
  return 0.;
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  const char* attName;
  const char* attValue;

  this->WCS->flag=-1;
  if((this->WCSStatus = wcsini(1, StringToInt(this->GetAttribute("SlicerAstro.NAXIS")), this->WCS)))
    {
    vtkErrorMacro("wcsini ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
    }

  std::string pre = "SlicerAstro.WCS.";
  std::string und = "UNDEFINED";
  std::string temp;
  int i, j, k;

  while (*atts != NULL)
    {
    attName = *(atts++);
    attValue = *(atts++);

    if (!strcmp(attName, "SpaceQuantities"))
      {
      std::istringstream f(attValue);
      std::string s;
      int i = 0;
      while (std::getline(f, s, ';'))
        {
        this->SetSpaceQuantity(i, s.c_str());
        i++;
        }
      continue;
      }

    if (!strcmp(attName, "Space"))
      {
      this->SetSpace(attValue);
      continue;
      }

    temp = pre + "naxis";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->naxis = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crpix" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->crpix[i] = StringToDouble(attValue);
        continue;
        }
      }

    k = 0;
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        temp = pre + "pc" + IntToString(k);
        if (!strcmp(attName, temp.c_str()))
          {
          this->WCS->pc[k] = StringToDouble(attValue);
          continue;
          }
        k++;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "cdelt" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->cdelt[i] = StringToDouble(attValue);
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crval" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->crval[i] = StringToDouble(attValue);
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "cunit" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        strcpy(this->WCS->cunit[i], attValue);
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "ctype" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        strcpy(this->WCS->ctype[i], attValue);
        continue;
        }
      }

    temp = pre + "lonpole";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->lonpole = StringToDouble(attValue);
      continue;
      }

    temp = pre + "latpole";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->latpole = StringToDouble(attValue);
      continue;
      }

    temp = pre + "restfrq";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->restfrq = StringToDouble(attValue);
      continue;
      }

    temp = pre + "restwav";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->restwav = StringToDouble(attValue);
      continue;
      }

    temp = pre + "npv";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->npv = StringToInt(attValue);
      continue;
      }

    temp = pre + "npvmax";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->npvmax = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->npv; i++)
      {
      temp = pre + "pvi" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
         (this->WCS->pv[i]).i = StringToInt(attValue);
        continue;
        }
      temp = pre + "pvvalue" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->pv[i].value = StringToDouble(attValue);
        continue;
        }
      }

    temp = pre + "nps";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->nps = StringToInt(attValue);
      continue;
      }

    temp = pre + "npsmax";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->npsmax = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->npv; i++)
      {
      temp = pre + "psi" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        (this->WCS->ps[i]).i = StringToInt(attValue);
        continue;
        }
      temp = pre + "psvalue" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        strcpy (this->WCS->ps[i].value, attValue);
        continue;
        }
      }

    k = 0;
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        temp = pre + "cd" + IntToString(k);
        if (!strcmp(attName, temp.c_str()))
          {
          this->WCS->cd[k] = StringToDouble(attValue);
          continue;
          }
        k++;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crota" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        this->WCS->crota[i] = StringToDouble(attValue);
        continue;
        }
      }

    temp = pre + "altlin";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->altlin = StringToInt(attValue);
      continue;
      }

    temp = pre + "velref";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->velref = StringToInt(attValue);
      continue;
      }

    temp = pre + "alt";
    if (!strcmp(attName, temp.c_str()))
      {
      strcpy(this->WCS->alt, attValue);
      continue;
      }

    temp = pre + "colnum";
    if (!strcmp(attName, temp.c_str()))
      {
      this->WCS->colnum = StringToInt(attValue);
      continue;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "colax" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))\
        {
        this->WCS->colax[i] = StringToInt(attValue);
        continue;
        }
      }

    temp = pre + "wcsname";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->wcsname) = '\0';
        }
      else
        {
        strcpy(this->WCS->wcsname, attValue);
        }
      continue;
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "cname" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if(!strcmp(attValue, und.c_str()))
          {
          *(this->WCS->cname[i]) = '\0';
          }
        else
          {
          strcpy(this->WCS->cname[i], attValue);
          }
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "crder" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if (!strcmp(attValue, und.c_str()))
          {
          this->WCS->crder[i] = 0.;
          }
        else
          {
          this->WCS->crder[i] = StringToDouble(attValue);
          }
        continue;
        }
      }

    for (i = 0; i < this->WCS->naxis; i++)
      {
      temp = pre + "csyer" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if (!strcmp(attValue, und.c_str()))
          {
          this->WCS->csyer[i] = 0.;
          }
        else
          {
          this->WCS->csyer[i] = StringToDouble(attValue);
          }
        continue;
        }
      }

    temp = pre + "radesys";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->radesys) = '\0';
        }
      else
        {
        strcpy(this->WCS->radesys, attValue);
        }
      continue;
      }

    temp = pre + "equinox";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->equinox = 0.;
        }
      else
        {
        this->WCS->equinox = StringToDouble(attValue);
        }
      continue;
      }

    temp = pre + "specsys";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->specsys) = '\0';
        }
      else
        {
        strcpy(this->WCS->specsys, attValue);
        }
      continue;
      }

    temp = pre + "ssysobs";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->ssysobs) = '\0';
        }
      else
        {
        strcpy(this->WCS->ssysobs, attValue);
        }
      continue;
      }

    temp = pre + "velosys";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->velosys = 0.;
        }
      else
        {
        this->WCS->velosys = StringToDouble(attValue);
        }
      continue;
      }

    temp = pre + "ssyssrc";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->ssyssrc) = '\0';
        }
      else
        {
        strcpy(this->WCS->ssyssrc, attValue);
        }
      continue;
      }

    temp = pre + "zsource";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->zsource = 0.;
        }
      else
        {
        this->WCS->zsource = StringToDouble(attValue);
        }
      continue;
      }

    for (i = 0; i < 3; i++)
      {
      temp = pre + "obsgeo" + IntToString(i);
      if (!strcmp(attName, temp.c_str()))
        {
        if (!strcmp(attValue, und.c_str()))
          {
          this->WCS->obsgeo[i] = 0.;
          }
        else
          {
          this->WCS->obsgeo[i] = StringToDouble(attValue);
          }
        continue;
        }
      }

    temp = pre + "dateobs";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->dateobs) = '\0';
        }
      else
        {
        strcpy(this->WCS->dateobs, attValue);
        }
      continue;
      }

    temp = pre + "dateavg";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        *(this->WCS->dateavg) = '\0';
        }
      else
        {
        strcpy(this->WCS->dateavg, attValue);
        }
      continue;
      }

    temp = pre + "mjdobs";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->mjdobs = 0.;
        }
      else
        {
        this->WCS->mjdobs = StringToDouble(attValue);
        }
      continue;
      }

    temp = pre + "mjdavg";
    if (!strcmp(attName, temp.c_str()))
      {
      if (!strcmp(attValue, und.c_str()))
        {
        this->WCS->mjdavg = 0.;
        }
      else
        {
        this->WCS->mjdavg = StringToDouble(attValue);
        }
      continue;
      }
  }

  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
    }

  this->WriteXML(std::cout,0);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
// Copy the node\"s attributes to this object.
// Does NOT copy: ID, FilePrefix, Name, VolumeID
void vtkMRMLAstroVolumeDisplayNode::Copy(vtkMRMLNode *anode)
{
  int disabledModify = this->StartModify();

  Superclass::Copy(anode);
  vtkMRMLAstroVolumeDisplayNode *node =
      vtkMRMLAstroVolumeDisplayNode::SafeDownCast(anode);

  this->UpdateImageDataPipeline();

  if (!node)
    {
    return;
    }

  this->SetInputImageDataConnection(node->GetInputImageDataConnection());
  this->SetSpaceQuantities(node->GetSpaceQuantities());
  this->SetSpace(node->GetSpace());
  this->SetAttribute("SlicerAstro.NAXIS", node->GetAttribute("SlicerAstro.NAXIS"));

  this->WCS->flag=-1;
  if ((this->WCSStatus = wcscopy(1, node->WCS, this->WCS)))
    {
    vtkErrorMacro("wcscopy ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
    this->SetWCSStatus(node->GetWCSStatus());
    }

  if ((this->WCSStatus = wcsset(this->WCS)))
    {
    vtkErrorMacro("wcsset ERROR "<<WCSStatus<<":\n"<<
                    "Message from "<<WCS->err->function<<
                    "at line "<<WCS->err->line_no<<" of file "<<WCS->err->file<<
                    ": \n"<<WCS->err->msg<<"\n");
    this->SetWCSStatus(node->GetWCSStatus());
   }

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
int vtkMRMLAstroVolumeDisplayNode::SetSpaceQuantity(int ind, const char *name)
{

  if (ind >= this->SpaceQuantities->GetNumberOfValues())
    {
    this->SpaceQuantities->SetNumberOfValues(ind+1);
    }

  vtkStdString SpaceQuantities(name);
  if (this->SpaceQuantities->GetValue(ind) != SpaceQuantities)
    {
    this->SpaceQuantities->SetValue(ind, SpaceQuantities);
    return 1;
    }
  return 0;

}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetPixelString(double *ijk)
{
  if (this->GetVolumeNode()->GetImageData() == NULL)
    {
    return "No Image";
    }

  for (int i = 0; i < 3; i++)
    {
    if(ijk[i] < 0 or ijk[i] >=  this->GetVolumeNode()->GetImageData()->GetDimensions()[i])
      {
      return "Out of Frame";
      }
    }

  int numberOfComponents = this->GetVolumeNode()->GetImageData()->GetNumberOfScalarComponents();
  if (numberOfComponents > 3)
    {
    std::string s = IntToString(numberOfComponents) + " components";
    return s.c_str();
    }

  std::string pixel;
  std::string type = this->GetVolumeNode()->GetAttribute("SlicerAstro.DATAMODEL");
  size_t found = type.find("MOMENTMAP");
  if (found!=std::string::npos)
    {
    double component = this->GetVolumeNode()->GetImageData()->
        GetScalarComponentAsDouble(ijk[0],ijk[1],ijk[2],0);

    pixel = DoubleToString(component) + " " + this->GetVolumeNode()->GetAttribute("SlicerAstro.BUNIT") +
            " " + this->GetVolumeNode()->GetAttribute("SlicerAstro.BTYPE");
    }
  else
    {
    vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
                this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
    if (selectionNode)
      {
      vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode("intensity");

      for(int i = 0; i < numberOfComponents; i++)
        {
        double component = this->GetVolumeNode()->GetImageData()->
            GetScalarComponentAsDouble(ijk[0],ijk[1],ijk[2],i);
        pixel += unitNode->GetDisplayStringFromValue(component);
        pixel += ",";
        }
        pixel.erase(pixel.size()-1);
      }
    }

  return pixel;
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValue(const double world,
                                                                     vtkMRMLUnitNode *node)
{
  std::string value = "";
  if(!node)
    {
    return value.c_str();
    }

  value = node->GetDisplayStringFromValue(world);

  std::string firstPrefix;
  std::string secondPrefix;
  if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds"))
    {
    firstPrefix = "\x27 ";
    secondPrefix = "\x22";
    }
  if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    firstPrefix = "m ";
    secondPrefix = "s";
    }
  if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
      !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    double fractpart, intpart, displayValue;
    std::string displayValueString;
    std::stringstream strstream;
    strstream.setf(ios::fixed,ios::floatfield);

    displayValue = node->GetDisplayValueFromValue(world);
    fractpart = modf(displayValue, &intpart);
    value = DoubleToString(intpart) + node->GetSuffix() + " ";

    fractpart = (modf(fractpart * 60., &intpart)) * 60.;
    displayValueString = DoubleToString(fabs(intpart));
    if(intpart < 10.)
      {
       displayValueString = " " + displayValueString;
      }
    value = value + displayValueString + firstPrefix;
    displayValueString = "";
    strstream.precision(node->GetPrecision());
    strstream << fabs(fractpart);
    strstream >> displayValueString;
    if(fractpart < 10.)
      {
      displayValueString = " " + displayValueString;
      }

    value = value + displayValueString + secondPrefix;
    }

  return value;
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueX(const double world)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
    return vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValue(world, unitNode);
    }
  return "";
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueY(const double world)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
    return vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValue(world, unitNode);
    }
  return "";
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValueZ(const double world)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
    return vtkMRMLAstroVolumeDisplayNode::GetDisplayStringFromValue(world, unitNode);
    }
  return "";
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::AddVelocityInfoToDisplayStringZ(std::string value)
{
  if (!this->SpaceQuantities->GetValue(2).compare("velocity"))
    {
    value = value + " (" + this->GetWCSStruct()->ctype[2] + ")";
    }
  return value;
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValue(const double world,
                                                                         vtkMRMLUnitNode *node)
{
  std::string value = "";
  if(!node)
    {
    return value.c_str();
    }

  std::string firstPrefix;
  std::string secondPrefix;
  std::string thirdPrefix;
  if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds"))
    {
    firstPrefix = "\u00B0 ";
    secondPrefix = "\x27 ";
    thirdPrefix = "\x22";
    }
  if (!strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    firstPrefix = "h ";
    secondPrefix = "m ";
    thirdPrefix = "s";
    }
  if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds") ||
      !strcmp(node->GetAttribute("DisplayHint"), "hoursAsMinutesSeconds"))
    {
    double fractpart, intpart, displayValue;
    std::string displayValueString;
    std::stringstream strstream;
    strstream.setf(ios::fixed,ios::floatfield);

    if (!strcmp(node->GetAttribute("DisplayHint"), "DegreeAsArcMinutesArcSeconds"))
      {
      displayValue = world;
      }
    else
      {
      displayValue = node->GetDisplayValueFromValue(world);
      }

    fractpart = modf(displayValue, &intpart);
    value = DoubleToString(intpart) + firstPrefix;

    fractpart = (modf(fractpart * 60., &intpart)) * 60.;
    if(fractpart > 59.999)
      {
      fractpart = 0.;
      intpart += 1.;
      }
    displayValueString = DoubleToString(fabs(intpart));
    if(intpart < 10.)
      {
       displayValueString = " " + displayValueString;
      }
    value = value + displayValueString + secondPrefix;
    displayValueString = "";
    strstream.precision(0);
    strstream << fabs(fractpart);
    strstream >> displayValueString;
    if(fractpart < 10.)
      {
      displayValueString = " " + displayValueString;
      }

    value = value + displayValueString + thirdPrefix;
    return value.c_str();
    }
  return node->GetDisplayStringFromValue(world);
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValueX(const double world)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(0));
    return vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValue(world, unitNode);
    }
  return "";
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValueY(const double world)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(1));
    return vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValue(world, unitNode);
    }
  return "";
}

//----------------------------------------------------------------------------
std::string vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValueZ(const double world)
{
  vtkMRMLSelectionNode* selectionNode =  vtkMRMLSelectionNode::SafeDownCast(
              this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (selectionNode)
    {
    vtkMRMLUnitNode* unitNode = selectionNode->GetUnitNode(this->SpaceQuantities->GetValue(2));
    return vtkMRMLAstroVolumeDisplayNode::GetAxisDisplayStringFromValue(world, unitNode);
    }
  return "";
}

//----------------------------------------------------------------------------
void vtkMRMLAstroVolumeDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  std::string quantities = "";
  int i,j,k;

  if (this->SpaceQuantities)
    {
    for(i = 0; i < this->SpaceQuantities->GetNumberOfValues(); i++)
      {
      quantities +=  this->SpaceQuantities->GetValue(i) + ";";
      }
    }

  os << indent << "SpaceQuantities=\"" << quantities << "\n";
  os << indent << "Space: " << (this->Space ? this->Space : "(none)") << "\n";

  std::string pre=" SlicerAstro.WCS.";
  std::string und="UNDEFINED";

  os << indent << pre << "flag:   " << this->WCS->flag <<std::endl;
  os << indent << pre << "naxis:   " << this->WCS->naxis <<std::endl;
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "crpix"<<i<<":   " << this->WCS->crpix[i] <<std::endl;
    }

  // Linear transformation.
  k = 0;
  for (i = 0; i < this->WCS->naxis; i++)
    {
    for (j = 0; j < this->WCS->naxis; j++)
      {
      os << indent << pre << "pc"<<k<<":   " << this->WCS->pc[k] <<std::endl;
      k++;
      }
    }

  // Coordinate increment at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "cdelt"<<i<<":   " << this->WCS->cdelt[i] <<std::endl;
    }

  // Coordinate value at reference point.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "crval"<<i<<":   " << this->WCS->crval[i] <<std::endl;
    }

  // Coordinate units and type.
  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "cunit"<<i<<":   " << this->WCS->cunit[i] <<std::endl;
    }

  for (i = 0; i < this->WCS->naxis; i++)
    {
    os << indent << pre << "ctype"<<i<<":   " << this->WCS->ctype[i] <<std::endl;
    }

  // Celestial and spectral transformation parameters.
  os << indent << pre << "lonpole:   " << this->WCS->lonpole <<std::endl;
  os << indent << pre << "latpole:   " << this->WCS->latpole <<std::endl;
  os << indent << pre << "restfrq:   " << this->WCS->restfrq <<std::endl;
  os << indent << pre << "restwav:   " << this->WCS->restwav <<std::endl;
  os << indent << pre << "npv:   " << this->WCS->npv <<std::endl;
  os << indent << pre << "npvmax:   " << this->WCS->npvmax <<std::endl;

  for (i = 0; i < this->WCS->npv; i++)
    {
    os << indent << pre << "pvi"<<i<<":   " << (this->WCS->pv[i]).i <<std::endl;
    os << indent << pre << "pvvalue"<<i<<":   " << (this->WCS->pv[i]).value <<std::endl;
    }

  os << indent << pre << "nps:   " << this->WCS->nps <<std::endl;
  os << indent << pre << "npsmax:   " << this->WCS->npsmax <<std::endl;

  for (i = 0; i < this->WCS->nps; i++)
    {
    os << indent << pre << "pvi"<<i<<":   " << (this->WCS->ps[i]).i <<std::endl;
    os << indent << pre << "pvvalue"<<i<<":   " << (this->WCS->ps[i]).value <<std::endl;
    }

  // Alternate linear transformations.
  k = 0;
  if (this->WCS->cd)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      for (j = 0; j < this->WCS->naxis; j++)
        {
        os << indent << pre << "cd"<<k<<":   " << this->WCS->cd[k] <<std::endl;
        k++;
        }
      }
    }

  if (this->WCS->crota)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "crota"<<i<<":   " << this->WCS->crota[i] <<std::endl;
      }
    }

  os << indent << pre << "altlin:   " << this->WCS->altlin <<std::endl;
  os << indent << pre << "velref:   " << this->WCS->velref <<std::endl;
  os << indent << pre << "alt:   " << this->WCS->alt <<std::endl;
  os << indent << pre << "colnum:   " << this->WCS->colnum <<std::endl;

  if (this->WCS->colax)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      os << indent << pre << "colax"<<i<<":   " << this->WCS->colax[i] <<std::endl;
      }
    }

  if (this->WCS->wcsname[0] == '\0')
    {
    os << indent << pre << "wcsname:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "wcsname:   " << this->WCS->wcsname <<std::endl;
    }

  if (this->WCS->cname)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (this->WCS->cname[i][0] == '\0')
        {
        os << indent << pre << "cname"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "cname"<<i<<":   " << this->WCS->cname[i] <<std::endl;
        }
      }
    }

  if (this->WCS->crder)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined(this->WCS->crder[i]))
        {
        os << indent << pre << "crder"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "crder"<<i<<":   " << this->WCS->crder[i] <<std::endl;
        }
      }
    }

  if (this->WCS->csyer)
    {
    for (i = 0; i < this->WCS->naxis; i++)
      {
      if (undefined(this->WCS->csyer[i]))
        {
        os << indent << pre << "csyer"<<i<<":   " << und <<std::endl;
        }
      else
        {
        os << indent << pre << "csyer"<<i<<":   " << this->WCS->csyer[i] <<std::endl;
        }
      }
    }

  if (this->WCS->radesys[0] == '\0')
    {
    os << indent << pre << "radesys:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "radesys:   " << this->WCS->radesys <<std::endl;
    }

  if (undefined(this->WCS->equinox))
    {
    os << indent << pre << "equinox:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "equinox:   " << this->WCS->equinox <<std::endl;
    }

  if (this->WCS->specsys[0] == '\0')
    {
    os << indent << pre << "specsys:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "specsys:   " << this->WCS->specsys <<std::endl;
    }

  if (this->WCS->ssysobs[0] == '\0')
    {
    os << indent << pre << "ssysobs:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "ssysobs:   " << this->WCS->ssysobs <<std::endl;
    }

  if (undefined(this->WCS->velosys))
    {
    os << indent << pre << "velosys:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "velosys:   " << this->WCS->velosys <<std::endl;
    }

  if (this->WCS->ssyssrc[0] == '\0')
    {
    os << indent << pre << "ssyssrc:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "ssyssrc:   " << this->WCS->ssyssrc <<std::endl;
    }

  if (undefined(this->WCS->zsource))
    {
    os << indent << pre << "zsource:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "zsource:   " << this->WCS->zsource <<std::endl;
    }

  for (i = 0; i < 3; i++)
    {
    if (undefined(this->WCS->obsgeo[i]))
      {
      os << indent << pre << "obsgeo"<<i<<":   " << und <<std::endl;
      }
    else
      {
      os << indent << pre << "obsgeo"<<i<<":   " << this->WCS->obsgeo[i] <<std::endl;
      }
    }

  if (this->WCS->dateobs[0] == '\0')
    {
    os << indent << pre << "dateobs:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "dateobs:   " << this->WCS->dateobs <<std::endl;
    }

  if (this->WCS->dateavg[0] == '\0')
    {
    os << indent << pre << "dateavg:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "dateavg:   " << this->WCS->dateavg <<std::endl;
    }

  if (undefined(this->WCS->mjdobs))
    {
    os << indent << pre << "mjdobs:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "mjdobs:   " << this->WCS->mjdobs <<std::endl;
    }

  if (undefined(this->WCS->mjdavg))
    {
    os << indent << pre << "mjdavg:   " << und <<std::endl;
    }
  else
    {
    os << indent << pre << "mjdavg:   " << this->WCS->mjdavg <<std::endl;
    }

}
