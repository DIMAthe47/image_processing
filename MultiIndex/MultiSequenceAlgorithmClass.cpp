#include "common.h"
#include "PriorityTuple.h"
#include <queue>

template <class T>
class MultiSequenceAlgorithm {
	//suppose each codebook has n_clusters codewords
private:
	const int n_subquantizers;
	const int n_clusters;
	const int* list_start_ndarray;
	const T* lists;
	const int lists_len;
	int* flatindex_multipliers;

	float PREV_PRIORITY = -1;

	void CHECK_ALGORITHM_ITSELF(bool* visited, int flatindex, float popped_priority) {
		if (visited[flatindex]) {
			fprintf(stderr, "flatindex: %d", flatindex);
			perror("cell already visited. Error in implementation");

			exit(1);
		}
		if (popped_priority < this->PREV_PRIORITY) {
			perror("popped prioirties must increase. Error in implementation");
			exit(1);
		}
		else {
			PREV_PRIORITY = popped_priority;
		}
	}

	template<typename T>
	void take_in_rows(const T* matrix, const int* columns, T* target_array, const int target_array_len) {
		for (int i = 0; i < target_array_len; i++) {
			int column = columns[i];
			target_array[i] = matrix[i*n_clusters + column];
		}
	}

	std::pair<int, int> find_list_sliceD(const int* list_start_ndarray, const int pos_in_list_start_ndarray) {
		int list_start = list_start_ndarray[pos_in_list_start_ndarray];

		int list_size;
		int next_list_start = list_start_ndarray[pos_in_list_start_ndarray + 1];
		list_size = next_list_start - list_start;

		std::pair<int, int> list_slice(list_start, list_start + list_size);

		return list_slice;
	}

	int flat_index(const int* multi_index) {
		int flatindex = 0;
		for (int dim = 0; dim < n_subquantizers; dim++) {
			flatindex += multi_index[dim] * flatindex_multipliers[dim];
		}
		return flatindex;
	}

	int flat_dim_offset(const int singular_dim) {
		int dim_offset = flatindex_multipliers[singular_dim];
		int flatindex = dim_offset;
		return flatindex;
	}


	bool check_cell_out_of_bounds(const int* multi_index) {
		for (int i = 0; i < n_subquantizers; i++) {
			if (multi_index[i] >= this->n_clusters) {
				return true;
			}
		}
		return false;
	}

	bool check_cell_already_visited(const int* multi_index, bool* visited) {
		int flatindex = flat_index(multi_index);
		bool already_visited = visited[flatindex];
		return already_visited;
	}

	bool check_surrounding_cells_visited(const int* multi_index,  bool* visited) {
		bool surrounding_cells_visited = true;
		for (int i = 0; i < n_subquantizers; i++) {
			bool is_border = multi_index[i] == 0;
			bool surrounding_cell_i_visited = is_border;
			if (!is_border) {
				int flatindex = flat_index(multi_index);
				int flatindex_inc = flat_dim_offset(i);
				flatindex -= flatindex_inc;
				surrounding_cell_i_visited = visited[flatindex];
			}
			surrounding_cells_visited = surrounding_cells_visited && surrounding_cell_i_visited;

			if (!surrounding_cell_i_visited)
				return false;
		}

		return surrounding_cells_visited;
	}

	bool check_for_push(bool *visited, const int* multi_index) {
		bool should_push = true;

		bool is_cell_already_visited = check_cell_already_visited(multi_index, visited);
		should_push = !is_cell_already_visited;

		if (!is_cell_already_visited) {
			bool is_out_of_bounds = check_cell_out_of_bounds(multi_index);
			should_push = !is_out_of_bounds;
			if (!is_out_of_bounds) {
				bool surrounding_cells_visited = check_surrounding_cells_visited(multi_index, visited);
				should_push = surrounding_cells_visited;
			}
		}

		return should_push;
	}

public:
	
	MultiSequenceAlgorithm(const int n_subquantizers, const int n_clusters, const int* list_start_ndarray, const T* lists, const int lists_len) :
		n_subquantizers(n_subquantizers), n_clusters(n_clusters), list_start_ndarray(list_start_ndarray),
		lists(lists), lists_len(lists_len) {

		flatindex_multipliers = new int[n_subquantizers];
		flatindex_multipliers[n_subquantizers - 1] = 1;
		for (int dim = n_subquantizers - 2; dim >= 0; dim--) {
			flatindex_multipliers[dim] = flatindex_multipliers[dim + 1] * n_clusters;
		}
	}

	//���� ���������� ��������������� ������� ����������
	void find_and_write_candidates(const float *cluster_distance_matrix, const int* nearest_cluster_index_matrix, T* candidate_list, const int candidate_list_len) {
		PREV_PRIORITY = -1;
		int m = this->n_subquantizers;

		int* _cluster_index_array = new int[m];
		float* _distances_array = new float[m];

		int* start_cell_multiindex = new int[m];
		memset(start_cell_multiindex, 0, sizeof(int)*m);

		int n_total_cells = 1;
		for (int i = 0; i < m; i++) {
			n_total_cells *= n_clusters;
		}
		bool* visited = new bool[n_total_cells];
		memset(visited, false, sizeof(bool)*n_total_cells);

		take_in_rows(nearest_cluster_index_matrix, start_cell_multiindex, _cluster_index_array, m);
		take_in_rows(cluster_distance_matrix, _cluster_index_array, _distances_array, m);
		float start_distance = sum_array(_distances_array, m);

		std::priority_queue<PriorityTuple<int*>, std::vector<PriorityTuple<int*>>, ComparePriorityTuple<int*>> min_heap;
		PriorityTuple<int*> start_element = { start_distance, start_cell_multiindex };
		min_heap.push(start_element);
		float PREV_PRIORITY = start_element.priority;

		int* nearest_cluster_index_array = new int[m];
		int total_candidates_to_take = candidate_list_len;
		int candidates_taken = 0;
		while (candidates_taken < total_candidates_to_take) {
			PriorityTuple<int*> nearest = min_heap.top();
			min_heap.pop();
			int* nearest_cell_multiindex = nearest.value;
			//printf("popped: %f ", nearest.priority);
			//print_multi_index(nearest_cell_multiindex, m);
			//printf("\n");
			int flatindex = flat_index(nearest_cell_multiindex);

			CHECK_ALGORITHM_ITSELF(visited, flatindex, nearest.priority);

			visited[flatindex] = true;
			take_in_rows(nearest_cluster_index_matrix, nearest_cell_multiindex, nearest_cluster_index_array, m);
			int pos_in_list_start_ndarray = flat_index(nearest_cluster_index_array);
			std::pair<int, int> list_slice = find_list_sliceD(list_start_ndarray, pos_in_list_start_ndarray);
			int copy_len = copy_array<T>(lists, candidate_list, list_slice, candidates_taken, total_candidates_to_take);
			candidates_taken += copy_len;
			//printf("candidates: ");
			//print_array(candidate_list, candidates_taken, "%d ");

			for (int dim = m - 1; dim >= 0; dim--) {
				int* next_cell_multi_index = new int[m];
				copy_array<int>(nearest_cell_multiindex, next_cell_multi_index, m);
				next_cell_multi_index[dim] += 1;

				bool can_push = check_for_push(visited, next_cell_multi_index);
				if (can_push) {
					take_in_rows(nearest_cluster_index_matrix, next_cell_multi_index, _cluster_index_array, m);
					take_in_rows(cluster_distance_matrix, _cluster_index_array, _distances_array, m);
					float next_cell_distance = sum_array(_distances_array, m);

					PriorityTuple<int*> next_consider_cell = { next_cell_distance,next_cell_multi_index };
					min_heap.push(next_consider_cell);

					//printf("	push: %f ", next_consider_cell.priority);
					//print_multi_index(next_consider_cell.value, m);
					//printf("\n");
				}
				else {
					delete[] next_cell_multi_index;
				}
			}
			delete[] nearest_cell_multiindex;
		}

	}
};

int main() {
	const float* cluster_distance_matrix = new const float[6]{ 2.0, 0., 2.0, 0., 2.0, 0., };
	const int* nearest_cluster_index_matrix = new const int[6]{ 1, 0,   1, 0,  1, 0, };
	int n_subquantizers = 3;
	int n_clusters = 2;

	//��������� ������� � �����
	int list_start_ndarray[] = { 0, 1, 2, 2, 4, 4, 6, 8,    10 };
	int lists[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
	int lists_len = 10;

	MultiSequenceAlgorithm<int> msa(n_subquantizers, n_clusters, list_start_ndarray, lists, lists_len);

	int* candidates = new int[10];
	msa.find_and_write_candidates(cluster_distance_matrix, nearest_cluster_index_matrix, candidates, 10);
	print_array(candidates, 10, "%d ");
	delete[] candidates;
	return 0;
}