/*
 * criaVTK.h
 *
 *  Created on: 22/02/2011
 *      Author: sachetto
 */

#ifndef CRIAVTK_H_
#define CRIAVTK_H_

#include<cstdio>
#include<cstdlib>
#include <vector>
#include <string>
#include <map>
#include<string>
#include <vtkSmartPointer.h>
#include <vtkPoints.h>
#include <vtkFloatArray.h>
#include <vtkDoubleArray.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkMergePoints.h>
#include <vtkPolyData.h>

void convertAlgToVTK3D(std::vector<std::string> files, bool binary, bool adaptive);
void convertAlgToVTK3D(std::vector<std::string> files, double sideLenght, bool binary);

#endif /* CRIAVTK_H_ */
