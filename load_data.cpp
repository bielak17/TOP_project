#include <iostream>
#include <string>
#include <cmath>

#include <fstream>
#include <sstream>
#include <vector>

#include "load_data.h"

using namespace std;

Result get_data() {
	int Nb_vertexes = 0;
	int Nb_vehicles = 0;
	int T_max = 0;
	Point* points = nullptr;
	string file_name;
	cout << " Podaj nazwe pliku z danymi: ";
	cin >> file_name;
	FILE* fp = nullptr;
	if (fopen_s(&fp, file_name.c_str(), "r") != 0) {
		cerr << "Error opening file: " << file_name << endl;
		exit(1);
	}
	char buffer[100];
	//get the first lines with data about number of vertexes, vehicles and Tmax
	fgets(buffer, 100, fp);
	sscanf_s(buffer, "n %d", &Nb_vertexes);
	fgets(buffer, 100, fp);
	sscanf_s(buffer, "m %d", &Nb_vehicles);
	fgets(buffer, 100, fp);
	sscanf_s(buffer, "tmax %d", &T_max);
	points = new Point[Nb_vertexes];
	int counter = 0;
	// get all the coordinates and weights of the vertexes till the end of the file
	while (fgets(buffer, 100, fp) != nullptr) {
		Point p;
		sscanf_s(buffer, "%f %f %d", &p.x, &p.y, &p.weight);
		points[counter] = p;
		counter++;
	}
	fclose(fp);
	// store weights of the vertexes in a separate array for easier access
	int* weights = new int[Nb_vertexes];
	for (int i = 0; i < Nb_vertexes; i++) {
		weights[i] = points[i].weight;
	}
	// calculate the distance matrix between all vertexes
	float** distance_matrix = new float* [Nb_vertexes];
	for (int i = 0; i < Nb_vertexes; i++) {
		distance_matrix[i] = new float[Nb_vertexes];
		for (int j = 0; j < Nb_vertexes; j++) {
			float dx = points[i].x - points[j].x;
			float dy = points[i].y - points[j].y;
			distance_matrix[i][j] = sqrt(dx * dx + dy * dy);
		}
	}
	Result r = { Nb_vertexes, Nb_vehicles, T_max, points, weights, distance_matrix };
	return r;
}