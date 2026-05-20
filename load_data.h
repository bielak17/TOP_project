#pragma once
#include <string>

//struct to store the coordinates and weight of each vertex
struct Point {
	float x;
	float y;
	int weight;
	//float weight;
};

//struct to store the data read from the file, including number of vertexes, vehicles, Tmax, the array of points, weights and distance matrix
struct Result {
	int vert;
	int veh;
	int Tmax;
	Point* points;
	int* weights;
	//float* weights;
	float** distance_matrix;
};

// function to get all the data from the file and store it in the Result struct
Result get_data();
Result get_data_AQ();
