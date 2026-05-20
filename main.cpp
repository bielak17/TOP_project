#include <ilcplex/ilocplex.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

#include "load_data.h"

using namespace std;

ILOSTLBEGIN

typedef IloArray<IloNumArray> NumMatrix;
typedef IloArray<IloBoolArray> BoolMatrix;
typedef IloArray<IloNumVarArray> NumVarMatrix;

int main()
{
	Result data = get_data();
	const IloInt nb_vertexes = data.vert;
	const IloInt nb_vehicles = data.veh;
	const IloInt Tmax = data.Tmax;
	const Point* points = data.points;
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
		// Testowe wypisanie wczytanych danych
		/*
		cout << "Dane: " << endl;
		cout << "Number of vertexes: " << nb_vertexes << endl;
		cout << "Number of vehicles: " << nb_vehicles << endl;
		cout << "Tmax: " << Tmax << endl;
		for (int i = 0; i < nb_vertexes; i++) {
			cout << "Vertex " << i << ": (" << data.points[i].x << ", " << data.points[i].y << "), weight: " << weights[i] << endl;
		}
		cout << "Odleglosc między wierzcholkami 0 i 1 (sqrt(2)): " << distance_matrix[0][1] << endl;
		cout << "Start vertex: (" << data.points[start_vertex].x << ", " << data.points[start_vertex].y << "), weight: " << weights[start_vertex] << endl;
		cout << "End vertex: (" << data.points[end_vertex].x << ", " << data.points[end_vertex].y << "), weight: " << weights[end_vertex] << endl;
		system("pause");
		*/
		/// RESTRICTED MASTER PROBLEM (RMP) ///

		IloModel masterModel(env);

		// Zmienne decyzyjne dla wierzchołków
		IloNumVarArray visited(env, nb_vertexes, 0.0, 1.0, ILOFLOAT);
		// Tablica tras (zmiennych RMP) generowanych dynamicznie
		IloNumVarArray route(env);
		vector<vector<int>> generated_routes; // Pamiec fizycznego przebiegu kazdej trasy potrzebna do wypisania

		// Funkcja celu (Maksymalizacja nagród)
		IloExpr objective_expr(env);
		for (int i = 0; i < nb_vertexes; i++) {
			objective_expr += weights[i] * visited[i];
		}
		IloObjective masterObj = IloAdd(masterModel, IloMaximize(env, objective_expr));
		objective_expr.end();

		// Ograniczenie (22): suma użytych tras wynosi dokładnie m (nb_vehicles)
		IloRange routes_count_const = IloAdd(masterModel, IloRange(env, nb_vehicles, nb_vehicles, "RoutesCount"));

		// Ograniczenia (21): powiązanie wierzchołków z trasami
		//vertex_visited_const[i] - ile tras odwiedza wierzchołek i. Jeśli i jest odwiedzony (visited[i] = 1), to musi być odwiedzony przez co najmniej jedną trasę (vertex_visited_const[i] >= 1).
		// Jeśli i nie jest odwiedzony (visited[i] = 0), to nie może być odwiedzony przez żadną trasę (vertex_visited_const[i] >= 0).
		// Może być odwiedzony przez więcej niż jedną trasę, ale to nie jest problemem,
		// bo w funkcji celu liczy się tylko czy jest odwiedzony (visited[i] = 1) czy nie (visited[i] = 0).
		IloRangeArray vertex_visited_const(env, nb_vertexes);
		for (int i = 0; i < nb_vertexes; i++) {
			if (i == start_vertex || i == end_vertex) {
				vertex_visited_const[i] = IloRange(env, -IloInfinity, IloInfinity); //ustawienie dla startu i mety tylko do wyrównania indeksów
			}
			else {
				vertex_visited_const[i] = IloAdd(masterModel, IloRange(env, 0.0, -visited[i], IloInfinity));
			}
		}

		// Dodanie trasy początkowej zapewniającej rozwiązanie - pusty przejazd od startu do mety dla każdego pojazdu (funkcja celu = 0).
		IloNumColumn init_col = routes_count_const(1);
		route.add(IloNumVar(init_col, 0.0, IloInfinity, ILOFLOAT));
		init_col.end();
		generated_routes.push_back({(int)start_vertex, (int)end_vertex}); // Zapisanie trywialnej drogi w pamięci

		IloCplex masterCplex(masterModel);
		// Nie wypisywanie niepotrzebnych logów podczas rozwiązywania problemu
		masterCplex.setOut(env.getNullStream());

		/// PRICING PROBLEM (PODPROBLEM) ///
		IloModel pricingModel(env);

		// Zmienne decyzyjne podproblemu
		// Ograniczenie (17, 18): - binarność zmiennych a[i] i x[i][j]
		// Nazwy zgodne z artykułem a - 'visited' wierzchołka, x - 'flow' na łuku i->j
		IloNumVarArray a(env, nb_vertexes, 0, 1, ILOINT); // Czy wierzchołek i jest odwiedzony?
		NumVarMatrix x(env, nb_vertexes); // Przepływ na łuku i -> j
		for (int i = 0; i < nb_vertexes; i++) {
			x[i] = IloNumVarArray(env, nb_vertexes, 0, 1, ILOINT);
		}
		IloNumVarArray s(env, nb_vertexes, 0.0, Tmax, ILOFLOAT); // czas dotarcia do wierzchołka od startu

		// Funkcja celu - Maksymalizacja zysków pomniejszona o "koszty" dualne (Reduced Cost) 
		IloExpr pricing_obj_expr(env); //placeholder - dokładne wartości obliczane w pętli column generation 
		IloObjective reducedCostObj = IloAdd(pricingModel, IloMaximize(env, pricing_obj_expr));

		// Ograniczenie (15): dokładnie jedna trasa wychodząca ze startu dla każdego podproblemu
		IloExpr start_out(env);
		for (int j = 1; j < nb_vertexes; j++) {
			start_out += x[start_vertex][j];
		}
		pricingModel.add(start_out == 1);
		start_out.end();

		// Ograniczenie (16): dokładnie jedna trasa zjeżdżająca na metę (n+1) dla każdego podproblemu
		IloExpr end_in(env);
		for (int i = 0; i < nb_vertexes - 1; i++) {
			end_in += x[i][end_vertex];
		}
		pricingModel.add(end_in == 1);
		end_in.end();

		pricingModel.add(a[start_vertex] == 1); // Start na pewno odwiedzony
		pricingModel.add(a[end_vertex] == 1); // Meta na pewno odwiedzona
		pricingModel.add(s[start_vertex] == 0); // Czas na starcie = 0

		// Ograniczenie dodatkowe: nikt nie wjeżdża do startu i nikt nie wyjeżdża z mety (przyspiesza działanie i usuwa pętle)
		for (int i = 0; i < nb_vertexes; i++) {
			pricingModel.add(x[i][start_vertex] == 0);
			pricingModel.add(x[end_vertex][i] == 0);
		}

		// Ograniczenie (12) i (13): przepływ w wierzchołkach pośrednich - jeśli wierzchołek jest odwiedzony, musi być dokładnie jeden wjazd i jeden wyjazd
		for (int i = 0; i < nb_vertexes; i++) {
			pricingModel.add(x[i][i] == 0); // Zakaz podróży sam do siebie
			if (i != start_vertex && i != end_vertex) {
				IloExpr in_flow(env);
				IloExpr out_flow(env);
				// sumujemy krawędzie wchodzące i wychodzące z i
				for (int j = 0; j < nb_vertexes; j++) {
					if (i != j) {
						in_flow += x[j][i];
						out_flow += x[i][j];
					}
				}
				// Jeśli a[i] = 1, to musi być dokładnie jeden wjazd i jeden wyjazd. Jeśli a[i] = 0, to nie może być żadnego wjazdu ani wyjazdu.
				// Zatem a[i] = ilość wjazdów = ilość wyjazdów.
				pricingModel.add(in_flow == a[i]);
				pricingModel.add(out_flow == a[i]);
				in_flow.end();
				out_flow.end();
			}
		}

		// Ograniczenie (14) ograniczenie na czas podróży (Czas podróży i Tmax)
		IloExpr total_time(env);
		IloNum max_dist = 0;
		for (int i = 0; i < nb_vertexes; i++) {
			for (int j = 0; j < nb_vertexes; j++) {
				if (distance_matrix[i][j] > max_dist) max_dist = distance_matrix[i][j];
			}
		}
		IloNum big_M = Tmax + max_dist; // Parametr duże M
		for (int i = 0; i < nb_vertexes; i++) {
			for (int j = 0; j < nb_vertexes; j++) {
				if (i != j) {
					// Jeśli x[i][j] = 1, to s[j] >= s[i] + distance_matrix[i][j]. Jeśli x[i][j] = 0, to big M zawsze gwarantuje prawdę.
					if (j != start_vertex && i != end_vertex) {
						// równanie z wykładu
						pricingModel.add(s[i] + distance_matrix[i][j] - s[j] <= big_M * (1 - x[i][j]));
					}
					total_time += distance_matrix[i][j] * x[i][j];
				}
			}
		}

		// Ograniczenie (5): suma czasów na ścieżce <= Tmax
		pricingModel.add(total_time <= Tmax);
		total_time.end();

		IloCplex pricingCplex(pricingModel);
		// Nie wypisywanie niepotrzebnych logów podczas rozwiązywania podproblemu
		pricingCplex.setOut(env.getNullStream());

		/// COLUMN-GENERATION PROCEDURE ///
		IloNumArray pi(env, nb_vertexes); // Ceny dualne dla węzłów
		auto start_time = chrono::steady_clock::now();
		for (int iter = 0; ; iter++) {
			auto current_time = chrono::steady_clock::now();
			chrono::duration<double> elapsed = current_time - start_time;
			cout << "--- Iteracja " << iter << " [" << fixed << setprecision(2) << elapsed.count() << " s] ---" << endl;
			// Rozwiązujemy master problem
			if (!masterCplex.solve()) {
				cout << "  [Master] Blad podczas rozwiazywania Master problemu!" << endl;
				break;
			}
			cout << "  [Master] Obecna wartosc funkcji celu (RMP): " << fixed << setprecision(2) << masterCplex.getObjValue() << endl;

			// 2. Pobieramy ceny dualne (pi) z ograniczeń odwiedzin węzłów
			IloNum pi0 = masterCplex.getDual(routes_count_const);
			for (int i = 0; i < nb_vertexes; i++) {
				if (i == start_vertex || i == end_vertex) {
					pi[i] = 0.0;
				} else {
					pi[i] = masterCplex.getDual(vertex_visited_const[i]);
				}
			}

			// 3. Aktualizujemy funkcję celu dla podproblemu (Pricing Problem)
			// Wzór: Zredukowany koszt = Suma po wierzchołkach a[i] * (- pi_i) - pi0
			IloExpr new_pricing_obj(env);
			for (int i = 0; i < nb_vertexes; i++) {
				if (i != start_vertex && i != end_vertex) {
					new_pricing_obj += (-pi[i]) * a[i];
				}
			}
			new_pricing_obj -= pi0;

			// Podmiana starej funkcji celu na nową z zauktalizowanymi cenami dualnymi
			reducedCostObj.setExpr(new_pricing_obj);
			new_pricing_obj.end();

			// 4. Rozwiązujemy podproblem szukając drogi, która najbardziej polepszy wynik
			if (!pricingCplex.solve()) {
				cout << "  [Pricing] Blad podczas rozwiazywania Pricing problemu!" << endl;
				break;
			}

			double reduced_cost_val = pricingCplex.getObjValue();
			cout << "  [Pricing] Zredukowany koszt znalezionej trasy: " << fixed << setprecision(4) << reduced_cost_val << endl;

			// Warunek końca: W column generation kończymy szukać, jeśli zredukowany koszt jest <= '0'
			if (reduced_cost_val <= 1e-6) {
				cout << "  -> Koniec generacji kolumn. Nie znaleziono zadnych poprawiajacych tras." << endl;
				break;
			}

			// 6. Jeżeli kolumna ma zredukowany koszt dodatni -> dodajemy nową trasę do RMP
			IloNumColumn new_col = masterObj(0) + routes_count_const(1);

			for (int i = 0; i < nb_vertexes; i++) {
				if (i != start_vertex && i != end_vertex) {
					if (pricingCplex.getValue(a[i]) > 0.5) { 
						new_col += vertex_visited_const[i](1);
					}
				}
			}

			// Odtworzenie fizycznej trasy z Pricing korzystając z krawędzi x[i][j] - tylko do wyświetlania
			vector<int> path;
			int curr = start_vertex;
			path.push_back(curr);
			int watchdog = 0;
			while (curr != end_vertex && watchdog < nb_vertexes) {
				bool found = false;
				for (int j = 0; j < nb_vertexes; j++) {
					if (curr != j && pricingCplex.getValue(x[curr][j]) > 0.5) {
						curr = j;
						path.push_back(curr);
						found = true;
						break;
					}
				}
				if (!found) break; // zapobiega zapetleniu
				watchdog++;
			}
			generated_routes.push_back(path);

			route.add(IloNumVar(new_col, 0.0, IloInfinity, ILOFLOAT));
			new_col.end();
			cout << "  [RMP] Dodano nowa kolumne (trase) do modelu RMP." << endl;
		}

		// Po zakończeniu Column Generation rozwiązujemy RMP wymuszając Integer (całkowitość zmiennych `route`) 
		// Ostateczne przełączenie RMP do MIP (tzw. Branch and Price, ale my uzyjemy wbudowanego szukacza calkowitego CPLEX)
		cout << "\n--- Koncowe rozwiazanie calkowitoliczbowe (Integer) ---" << endl;
		masterModel.add(IloConversion(env, route, ILOINT));

		if (masterCplex.solve()) {
			cout << "Solution status: " << masterCplex.getStatus() << endl;
			cout << " Maksymalna punktacja podrozy = " << fixed << setprecision(0) << masterCplex.getObjValue() << endl;

			cout << "\nZnalezione trasy (" << nb_vehicles << " pojazdow):" << endl;
			int vehicle_id = 1;
			for (int j = 0; j < route.getSize(); j++) {
				// Sprawdzamy czy RMP (MIP) ostatecznie wybrał tę trasę
				int times_used = round(masterCplex.getValue(route[j]));
				for (int k = 0; k < times_used; k++) {
					cout << "  Pojazd " << vehicle_id++ << ": ";
					for (size_t p = 0; p < generated_routes[j].size(); p++) {
						cout << generated_routes[j][p];
						if (p < generated_routes[j].size() - 1) cout << " -> ";
					}
					cout << endl;
				}
				if (vehicle_id > nb_vehicles) break; // Zabezpieczenie przed wypisaniem nadmiaru
			}
		}
		else {
			cout << " No solution found" << endl;
		}
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
