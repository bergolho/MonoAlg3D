/*
 * criaVTK.cpp
 *
 *  Created on: 22/02/2011
 *      Author: sachetto
 */

/*
 * main.cpp
 *
 *  Created on: 22/02/2011
 *      Author: sachetto
 */

#include "criaVTK.h"
#include "file_util.h"
#include "hash/point_hash.h"
#include <cstring>
#include <set>

using namespace std;

void split(const std::string &str, std::vector<std::string> &tokens,
           const std::string &delimiters = " ") {
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos) {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

inline bool get_file_values(ifstream *solution, double *centerx,
                            double *centery, double *centerz, double *halfl,
                            double *v, bool binary) {

    if (binary) {
        solution->read(reinterpret_cast<char *>(centerx), sizeof(*centerx));
        solution->read(reinterpret_cast<char *>(centery), sizeof(*centery));
        solution->read(reinterpret_cast<char *>(centerz), sizeof(*centerz));
        solution->read(reinterpret_cast<char *>(halfl), sizeof(*halfl));
        solution->read(reinterpret_cast<char *>(v), sizeof(*v));

        return solution->gcount() == 0 ? false : true;

    } else {
        string line;
        if (getline(*solution, line)) {
            std::vector<std::string> aux;
            split(line, aux, ",");
            *centerx = atof(aux[0].c_str());
            *centery = atof(aux[1].c_str());
            *centerz = atof(aux[2].c_str());
            *halfl = atof(aux[3].c_str());
            *v = atof(aux[4].c_str());
            return true;
        }
        else {
            return false;
        }
    }
}

void convertAlgToVTK3D(std::vector<std::string> files, bool binary, bool adaptive) {

    std::stringstream path(std::stringstream::in | std::stringstream::out);
    int pos = files[0].find_last_of("/");
    path << files[0].substr(0, pos);
    std::string savePath = path.str() + "/vtu/";

    std::cout << "Creating dir: " << savePath << std::endl;
    createDir(savePath.c_str());

    static int t_counter = 0;
    ulong size = files.size();

    vtkSmartPointer<vtkDoubleArray> values;
    vtkSmartPointer<vtkCellArray> cells;
    vtkSmartPointer<vtkPoints> initialPoints;

    point_hash *hash;

    bool meshIsRead = false;

    for (unsigned int i = 0; i < size; i++) {
        values = vtkSmartPointer<vtkDoubleArray>::New();

        if (adaptive || !meshIsRead) {
            cells = vtkSmartPointer<vtkCellArray>::New();
            initialPoints = vtkSmartPointer<vtkPoints>::New();
            hash = point_hash_create();
        }

        double centerx, centery, centerz, halfl, v;

        std::ifstream solution;

        if (binary) {
            solution.open(files[i].c_str(), ios::in | ios::binary);

        } else {
            solution.open(files[i].c_str());
        }

        std::string line;

        std::stringstream fileName(std::stringstream::in | std::stringstream::out);

        fileName << files[i].substr(pos + 1, files[i].length());
        std::string fullPath = path.str() + "/vtu/" + fileName.str() + ".vtu";

        std::cout << "Converting " << files[i] << " to " << fullPath << std::endl;

        vtkIdType pointId;

        point_3d aux1;
        point_3d aux2;
        point_3d aux3;
        point_3d aux4;
        point_3d aux5;
        point_3d aux6;
        point_3d aux7;
        point_3d aux8;

        bool insert[8];

        vtkIdType id;


        while (get_file_values(&solution,&centerx, &centery, &centerz, &halfl, &v, binary)) {

            values->InsertNextValue(v);

            aux1.x = centerx - halfl;
            aux1.y = centery - halfl;
            aux1.z = centerz - halfl;

            aux2.x = centerx + halfl;
            aux2.y = centery - halfl;
            aux2.z = centerz - halfl;

            aux3.x = centerx + halfl;
            aux3.y = centery + halfl;
            aux3.z = centerz - halfl;

            aux4.x = centerx - halfl;
            aux4.y = centery + halfl;
            aux4.z = centerz - halfl;

            aux5.x = centerx - halfl;
            aux5.y = centery - halfl;
            aux5.z = centerz + halfl;

            aux6.x = centerx + halfl;
            aux6.y = centery - halfl;
            aux6.z = centerz + halfl;

            aux7.x = centerx + halfl;
            aux7.y = centery + halfl;
            aux7.z = centerz + halfl;

            aux8.x = centerx - halfl;
            aux8.y = centery + halfl;
            aux8.z = centerz + halfl;

            if (adaptive || !meshIsRead) {
                if (point_hash_search(hash, aux1) == 0) {
                    id = initialPoints->InsertNextPoint(aux1.x, aux1.y, aux1.z);
                    point_hash_insert(hash, aux1, id);
                }

                if (point_hash_search(hash, aux2) == 0) {
                    id = initialPoints->InsertNextPoint(aux2.x, aux2.y, aux2.z);
                    point_hash_insert(hash, aux2, id);
                }

                if (point_hash_search(hash, aux3) == 0) {
                    id = initialPoints->InsertNextPoint(aux3.x, aux3.y, aux3.z);
                    point_hash_insert(hash, aux3, id);
                }

                if (point_hash_search(hash, aux4) == 0) {
                    id = initialPoints->InsertNextPoint(aux4.x, aux4.y, aux4.z);
                    point_hash_insert(hash, aux4, id);
                }

                if (point_hash_search(hash, aux5) == 0) {
                    id = initialPoints->InsertNextPoint(aux5.x, aux5.y, aux5.z);
                    point_hash_insert(hash, aux5, id);
                }

                if (point_hash_search(hash, aux6) == 0) {
                    id = initialPoints->InsertNextPoint(aux6.x, aux6.y, aux6.z);
                    point_hash_insert(hash, aux6, id);
                }

                if (point_hash_search(hash, aux7) == 0) {
                    id = initialPoints->InsertNextPoint(aux7.x, aux7.y, aux7.z);
                    point_hash_insert(hash, aux7, id);
                }

                if (point_hash_search(hash, aux8) == 0) {
                    id = initialPoints->InsertNextPoint(aux8.x, aux8.y, aux8.z);
                    point_hash_insert(hash, aux8, id);
                }

                cells->InsertNextCell(8);

                cells->InsertCellPoint(point_hash_search(hash, aux1));
                cells->InsertCellPoint(point_hash_search(hash, aux2));
                cells->InsertCellPoint(point_hash_search(hash, aux3));
                cells->InsertCellPoint(point_hash_search(hash, aux4));
                cells->InsertCellPoint(point_hash_search(hash, aux5));
                cells->InsertCellPoint(point_hash_search(hash, aux6));
                cells->InsertCellPoint(point_hash_search(hash, aux7));
                cells->InsertCellPoint(point_hash_search(hash, aux8));
            }

        }

        meshIsRead = true;

        {
            vtkSmartPointer<vtkUnstructuredGrid> ug =
                    vtkSmartPointer<vtkUnstructuredGrid>::New();

            ug->SetPoints(initialPoints);
            ug->SetCells(VTK_HEXAHEDRON, cells);

            values->SetName("Vm");
            ug->GetCellData()->SetScalars(values);
            ug->GetCellData()->SetActiveScalars("Vm");

            vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer =
                    vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();

            writer->SetInputData(ug);
            writer->SetFileName(fullPath.c_str());
            writer->Write();
        }
        if (adaptive || !meshIsRead)
            point_hash_destroy(hash);
    }
}


void
convertAlgToVTK3D(std::vector<std::string> files, double sideLenght, bool
binary) {

    std::stringstream path (std::stringstream::in | std::stringstream::out);
    int pos = files[0].find_last_of("/");
    path << files[0].substr(0, pos);
    std::string savePath = path.str()+"/vtu/";

    std::cout << "Creating dir: " << savePath << std::endl;
    createDir(savePath.c_str());

    static int t_counter = 0;
    ulong size = files.size();

    for (unsigned int i = 0; i < size; i++) {

        vtkSmartPointer<vtkDoubleArray> values =
                vtkSmartPointer<vtkDoubleArray>::New(); vtkSmartPointer<vtkMergePoints>
                pointLocator = vtkSmartPointer<vtkMergePoints>::New();
        vtkSmartPointer<vtkCellArray> cells =
                vtkSmartPointer<vtkCellArray>::New();

        vtkSmartPointer<vtkPolyData> points =
                vtkSmartPointer<vtkPolyData>::New(); vtkSmartPointer<vtkPoints> initialPoints =
                vtkSmartPointer<vtkPoints>::New();

        double aux_p[3];

        aux_p[0] = 0.0;
        aux_p[1] = 0.0;
        aux_p[2] = 0.0;
        initialPoints->InsertNextPoint(aux_p);

        aux_p[0] = sideLenght;
        aux_p[1] = 0.0;
        aux_p[2] = 0.0;
        initialPoints->InsertNextPoint(aux_p);

        aux_p[0] = 0.0;
        aux_p[1] = sideLenght;
        aux_p[2] = 0.0;
        initialPoints->InsertNextPoint(aux_p);

        aux_p[0] = sideLenght;
        aux_p[1] = sideLenght;
        aux_p[2] = 0.0;
        //////////////
        aux_p[0] = 0.0;
        aux_p[1] = 0.0;
        aux_p[2] = sideLenght;
        initialPoints->InsertNextPoint(aux_p);

        aux_p[0] = sideLenght;
        aux_p[1] = 0.0;
        aux_p[2] = sideLenght;
        initialPoints->InsertNextPoint(aux_p);

        aux_p[0] = 0.0;
        aux_p[1] = sideLenght;
        aux_p[2] = sideLenght;
        initialPoints->InsertNextPoint(aux_p);

        aux_p[0] = sideLenght;
        aux_p[1] = sideLenght;
        aux_p[2] = sideLenght;

        initialPoints->InsertNextPoint(aux_p);

        points->SetPoints(initialPoints);

        pointLocator->InitPointInsertion(points->GetPoints(), points->GetBounds());

        double centerx, centery, centerz, halfl, v;

        std::ifstream solution;

        if(binary) {
            solution.open(files[i].c_str(), ios::in | ios::binary);

        }
        else {
            solution.open(files[i].c_str());

        }

        std::string line;

        std::stringstream fileName (std::stringstream::in |
                                    std::stringstream::out);

        fileName << files[i].substr(pos+1, files[i].length());
        std::string fullPath = path.str()+"/vtu/"+fileName.str()+".vtu";

        std::cout << "Converting " << files[i] << " to " << fullPath <<
                  std::endl;

        vtkIdType pointId;

        double aux1[3];
        double aux2[3];
        double aux3[3];
        double aux4[3];
        double aux5[3];
        double aux6[3];
        double aux7[3];
        double aux8[3];

        int count = 1;

        while ( get_file_values(&solution,&centerx, &centery, &centerz, &halfl, &v, binary)) {

//            std::vector<std::string> aux;
//            split(line, aux, ",");
//            centerx = atof(aux[0].c_str());
//            centery = atof(aux[1].c_str());
//            centerz = atof(aux[2].c_str());
//            halfl = atof(aux[3].c_str());
//            v = atof(aux[4].c_str());




            values->InsertNextValue(v);

            aux1[0] = centerx - halfl;
            aux1[1] = centery - halfl;
            aux1[2] = centerz - halfl;

            pointLocator->InsertUniquePoint(aux1, pointId);

            aux2[0] = centerx + halfl;
            aux2[1] = centery - halfl;
            aux2[2] = centerz - halfl;

            pointLocator->InsertUniquePoint(aux2, pointId);

            aux3[0] = centerx + halfl;
            aux3[1] = centery + halfl;
            aux3[2] = centerz - halfl;

            pointLocator->InsertUniquePoint(aux3, pointId);

            aux4[0] = centerx - halfl;
            aux4[1] = centery + halfl;
            aux4[2] = centerz - halfl;

            pointLocator->InsertUniquePoint(aux4, pointId);
            ////////////////////////////////////////
            aux5[0] = centerx - halfl;
            aux5[1] = centery - halfl;
            aux5[2] = centerz + halfl;


            pointLocator->InsertUniquePoint(aux5, pointId);

            aux6[0] = centerx + halfl;
            aux6[1] = centery - halfl;
            aux6[2] = centerz + halfl;

            pointLocator->InsertUniquePoint(aux6, pointId);

            aux7[0] = centerx + halfl;
            aux7[1] = centery + halfl;
            aux7[2] = centerz + halfl;


            pointLocator->InsertUniquePoint(aux7, pointId);

            aux8[0] = centerx - halfl;
            aux8[1] = centery + halfl;
            aux8[2] = centerz + halfl;

            pointLocator->InsertUniquePoint(aux8, pointId);

            cells->InsertNextCell(8);
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux1));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux2));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux3));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux4));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux5));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux6));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux7));
            cells->InsertCellPoint(pointLocator->IsInsertedPoint(aux8));

        }

        vtkSmartPointer<vtkUnstructuredGrid> ug =
                vtkSmartPointer<vtkUnstructuredGrid>::New();

        ug->SetPoints(points->GetPoints());
        ug->SetCells(VTK_HEXAHEDRON, cells);


        values->SetName("Vm");
        ug->GetCellData()->SetScalars(values);
        ug->GetCellData()->SetActiveScalars("Vm");

        vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer =
                vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();

        writer->SetInputData(ug);
        writer->SetFileName(fullPath.c_str());
        writer->Write();

    }
}

