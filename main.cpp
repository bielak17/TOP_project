#include <ilcplex/ilocplex.h>
#include <iostream>
#include <string>

using namespace std;
//struct to store the coordinates and weight of each vertex
struct Point {
	float x;
	float y;
	int weight;
};
//struct to store the data read from the file, including number of vertexes, vehicles, Tmax, the array of points, weights and distance matrix
struct Result {
	int vert;
	int veh;
	int Tmax;
	Point* points;
	int* weights;
	float** distance_matrix;
};

// function to get all the data from the file and store it in the Result struct
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

ILOSTLBEGIN

typedef IloArray<IloNumArray> NumMatrix;

const IloInt nbperiods = 5;
const IloInt nbgens = 3;

int main()
{
	Result data = get_data();
	const IloInt nb_vertexes = data.vert;
	const IloInt nb_vehicles = data.veh;
	const IloInt Tmax = data.Tmax;
	const IloInt start_vertex = 0;
	const IloInt end_vertex = nb_vertexes-1;
    IloEnv env;
	try {
		// Główne dane problemu
		IloNumArray weights(env, nb_vertexes);
		for (int i = 0; i < nb_vertexes; i++) {
			weights[i] = data.weights[i];
		}
		NumMatrix distance_matrix(env, nb_vertexes);
		for (int i = 0; i < nb_vertexes; i++) {
			distance_matrix[i] = IloNumArray(env, nb_vertexes);
			for (int j = 0; j < nb_vertexes; j++) {
				distance_matrix[i][j] = data.distance_matrix[i][j];
			}
		}

		// Zmienne decyzyjne
		cout << "Dane: " << endl;
		cout << "Number of vertexes: " << nb_vertexes << endl;
		cout << "Number of vehicles: " << nb_vehicles << endl;
		cout << "Tmax: " << Tmax << endl;
		for (int i = 0; i < nb_vertexes; i++) {
			cout << "Vertex " << i << ": (" << data.points[i].x << ", " << data.points[i].y << "), weight: " << weights[i] << endl;
		}
		cout << distance_matrix[0][1] << endl;
		cout << "Start vertex: (" << data.points[start_vertex].x << ", " << data.points[start_vertex].y << "), weight: " << weights[start_vertex] << endl;
		cout << "End vertex: (" << data.points[end_vertex].x << ", " << data.points[end_vertex].y << "), weight: " << weights[end_vertex] << endl;
	}
    catch (IloException& ex) {
        cerr << "Error: " << ex << endl;
    }
    catch (...) {
        cerr << "Error" << endl;
    }
    env.end();
    return 0;
}
