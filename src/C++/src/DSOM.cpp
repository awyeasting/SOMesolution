#include "DSOM.h"
#include <float.h>
/* 
	Construct untrained SOM with given lattice width and height
*/
DSOM::DSOM(unsigned int width, unsigned int height)
{
	this->_width = width;
	this->_height = height;
}

/*
	Construct SOM from a saved SOM width, height, and set of weights
*/
DSOM::DSOM(std::istream &in) {
	this->load_weights(in);
}

/*
	Generates a random set of training data if there is no input file given
*/

double* DSOM::generateRandomTrainingInputs(unsigned int examples, unsigned int dimensions, int seedValue)
{
	double *returnData = new double [examples * dimensions];
	srand(seedValue);
	for (int i = 0; i < examples; i++)
	{
		int rowMod = (examples - i - 1)*dimensions;
		for (int j = 0; j < dimensions; j++)
		{
			double weight = SOM::randWeight();
			returnData[rowMod+j] = weight;
		}
	}

	for(int i = 0; i < examples; i++){
		int rowMod = (examples-i-1)*dimensions;
		for(int j = 0; j < dimensions; j++){
		}
	}
	return returnData;
}

/*
	Load a set of training data from a given filename
*/
double* DSOM::loadTrainingData(std::fstream& in, unsigned int& rows, unsigned int& cols, int read_count, double* featureMaxes, double* featureMins, bool flag) {
	
	// Read the first line to obtain the number of columns (dimensions) in the training data
	std::string line;
	std::getline(in, line);

	std::stringstream ss(line);
	std::vector<double> line1;
	double temp;
	int cols_count = 0;
	while (ss >> temp && cols_count < cols) {
		if (temp > featureMaxes[cols_count])
		{
			featureMaxes[cols_count] = temp;
		}
		if (temp < featureMins[cols_count])
		{
			featureMins[cols_count] = temp;
		}
		cols_count++;
		line1.push_back(temp);
	}

	std::vector<double*> lines;

	// Store first line in dynamic array and put into the vector of rows
	double* tempLine1 = new double[cols];
	for (int j = 0; j < cols; j++) {
		tempLine1[cols - j - 1] = line1.back();
		line1.pop_back();
		
	}
	lines.push_back(tempLine1);

	// Read all numbers into cols dimensional arrays added to the rows list
	int i = 0;
	double* unpackedLine = NULL;

	for(int linesNum = 1; linesNum < read_count * cols; linesNum++){
		in >> temp;
		if (!unpackedLine) {
			unpackedLine = new double[cols];
		}
		unpackedLine[i] = temp;
		if (temp > featureMaxes[i])
		{
			featureMaxes[i] = temp;
		}
		if (temp < featureMins[i])
		{
			featureMins[i] = temp;
		}
		i++;
		if (i == cols) {
			if(flag){
				in >> temp;
			}
				
			lines.push_back(unpackedLine);
			i = 0;
			unpackedLine = NULL;
		}
	}
	
	// Convert vector of arrays into 1d array of examples
	rows = lines.size();
	double* res = new double[rows * cols];
	for (i = 0; i < rows; i++) {
		double* temp = lines.back();
		int rowMod = (rows-i-1)*cols;
		for (int j = 0; j < cols; j++) {
			res[rowMod + j] = temp[j];
		}
		lines.pop_back();
		free(temp);
	}


	return res;
}

/*
	Every process would run this
*/
void DSOM::train_one_epoch(double* localMap, double* train_data, double* numerators, double* denominators, int num_examples, double initial_map_radius, int epoch, double time_constant)
{
	double* D = (double *)malloc(num_examples * _width * _height * sizeof(double));
	double* m_sq = (double *)malloc(_width * _height * sizeof(double));
	double* x_sq = (double *)malloc(num_examples * sizeof(double));
	int* BMUs = (int *)malloc(num_examples * sizeof(int));
	double* H = (double *)malloc(num_examples * _width * _height * sizeof(double));
	double neighborhood_radius;
	neighborhood_radius = initial_map_radius * exp(-double(epoch)/time_constant);

	// Find BMUs for every input instance
	// D = X_sq - 2X^TM + M_sq
	// D (xdn * nn)
	// Calc m_sq
	SqDists(localMap, _width * _height, _dimensions, m_sq);
	
	// Calc x_sq
	#ifdef ENABLE_OPENMP
	#pragma omp parallel for
	#endif
	SqDists(train_data, num_examples, _dimensions, x_sq);

	//Calculate D matrix
	#ifdef ENABLE_OPENMP
	#pragma omp parallel for
	#endif
	for (int j = 0; j < num_examples; j++) {
		for (int i = 0; i < _width * _height; i++) {
			// Calc x^Tm
			double xm = 0;
			for (int d = 0; d < _dimensions; d++) {
				xm += train_data[j * _dimensions + d] * localMap[i * _dimensions + d];
			}
			// Combine all
			D[j * _width * _height + i] = x_sq[j] - 2 * xm + m_sq[i];
		}
	}
	
	// BMU index of each training instance
	#ifdef ENABLE_OPENMP
	#pragma omp parallel for
	#endif
	for (int j = 0; j < num_examples; j++) {
		BMUs[j] = 0;
		for (int i = 1; i < _width * _height; i++) {
			if (D[j * _width * _height + i] < D[j * _width * _height + BMUs[j]]) {
				BMUs[j] = i;
			}
		}
	}
	// Calc gaussian function 
	// (num_examples x num nodes)
	#ifdef ENABLE_OPENMP
	#pragma omp parallel for
	#endif
	for (int j = 0; j < num_examples; j++) {
		for (int i = 0; i < _width * _height; i++) {
			H[j*_width*_height + i] = h(j, i, initial_map_radius, neighborhood_radius, BMUs);
		}
	}
	// Left multiply H by a num_examples dimensional vector of ones
	#ifdef ENABLE_OPENMP
	#pragma omp parallel for
	#endif
	for (int i = 0; i < _width * _height; i++) {
		denominators[i] = 0.0;
		for (int j = 0; j < num_examples; j++) {
			denominators[i] += H[j*_width*_height + i];
		}
	}
	
	//Calculate numerators
	#ifdef ENABLE_OPENMP
	#pragma omp parallel for
	#endif
	for (int i = 0; i < _width * _height; i++) {
		for (int d = 0; d < _dimensions; d++) {
			numerators[i * _dimensions + d] = 0.0;
			for (int j = 0; j < num_examples; j++) {
				numerators[i*_dimensions + d] += H[j*_width*_height + i] * train_data[j*_dimensions + d];
			}
		}
	}
	free(D);
	free (m_sq);
	free (x_sq);
	free(H);
	free(BMUs);
}

std::fstream& DSOM::GotoLine(std::fstream& file, unsigned int num){
    file.seekg(std::ios::beg);
    for(int i=0; i < num - 1; ++i){
        file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }
    return file;
}

/*
	Train the SOM using a set of training data over a given number of epochs with a given learning rate
*/
void DSOM::train_data(char* fileName, int fileSize, unsigned int current_rank, unsigned int num_procs, unsigned int epochs, unsigned int dimensions, unsigned int rowCount, int rank_seed, unsigned int map_seed, bool flag)
{
	double * train_data;
	int start, shift, read_count;
	double* global_max= (double *)malloc(sizeof(double) * dimensions);
	double* global_min=(double *)malloc(sizeof(double) * dimensions);
	//Where we load in the file.
	this->_dimensions = dimensions;
	start = ((rowCount / num_procs) * current_rank) + 1;
	read_count = rowCount / num_procs;
	
	auto start2 = std::chrono::high_resolution_clock::now();	

	if (current_rank >= (num_procs - (rowCount %num_procs)))
	{
		shift = current_rank - (num_procs - (rowCount % num_procs));
		start += shift;
		read_count += 1;
	}
	if (fileSize <= 0)
	{
		int current_rank_seed;
		//Rank 0 create seed value array. Scatter to current_rank_seed.
		train_data = generateRandomTrainingInputs(read_count, dimensions, rank_seed);
	}
	else
	{
		std::fstream in(fileName, std::ios::in | std::ios::out);

		if (!in.is_open()) {
			std::cout << "Invalid training data file '" << fileName << "'" << std::endl;
		}

		_featureMaxes = (double *)malloc(sizeof(double) * dimensions);
		_featureMins = (double*)malloc(sizeof(double) * dimensions);

		for(int i =0; i < dimensions; i++){
			_featureMaxes[i] = -1;
			_featureMins[i] = 10000;
		}

		std::fstream& file = GotoLine(in, start);
		//Need to do reading with localmaxes and localMins.
		train_data = loadTrainingData(file, rowCount, dimensions, read_count, _featureMaxes, _featureMins, flag);

		MPI_Barrier(MPI_COMM_WORLD);


		//RANK 0 Reduces, 
		// Allreduce Maxes
		MPI_Allreduce(_featureMaxes, global_max, dimensions, MPI_DOUBLE , MPI_MAX, MPI_COMM_WORLD);
		// Reduce Mins
		MPI_Allreduce(_featureMins, global_min, dimensions, MPI_DOUBLE , MPI_MIN, MPI_COMM_WORLD);

		
		//MPI BARRIER not sure if this is needed, because I think All_reduce is blocking.
		MPI_Barrier(MPI_COMM_WORLD);
		this->_dimensions = dimensions;
		normalizeData(train_data, read_count, global_max, global_min);

	}

	auto stop2 = std::chrono::high_resolution_clock::now();
	auto duration2 = std::chrono::duration_cast<std::chrono::duration<double>>(stop2 - start2);
	MPI_Barrier(MPI_COMM_WORLD);

	//Rank 0 needs to do the initalization of the map.
	if (current_rank == 0)
	{
		srand(map_seed);
		this->_weights = (double *)malloc(_width * _height * _dimensions * sizeof(double));
		for (int i = 0; i < _width; i++) {
			for (int j = 0; j < _height; j++) {
				for (int d = 0; d < _dimensions; d++) {
					double newWeight = randWeight();
					this->_weights[calcIndex(i,j,d)] = newWeight;
				}
			}
		}
	}
	

	// Calc initial map radius and time constant.
	double initial_map_radius = _width < _height ? ((double)_width) / 2.0 : ((double)_height) / 2.0;
	double time_constant = double(epochs) / log(initial_map_radius);

	//local_map is the variable used to broadcast, and modify each process.
	//local_numerators  and local_denominators are the variables used to pass to train_one_epoch and then be reduced
	//global_numerators/denominators are the variables used by rank 0 to reduce the local, and then update the map with.
	double* local_map = (double *)malloc(_width * _height * _dimensions * sizeof(double));
	double* local_numerators = (double*)malloc(_width * _height * _dimensions * sizeof(double));
	double* local_denominators = (double*)malloc(_width * _height * sizeof(double));
	double* global_numerators;
	double* global_denominator;
	//Have rank 0 allocate the memory for global num and denom as it will be doing the updating.

	if (current_rank == 0){
		global_numerators = (double*)malloc(_width * _height * _dimensions*sizeof(double));
		global_denominator = (double *)malloc(_width * _height * sizeof(double));
	}

	MPI_Barrier(MPI_COMM_WORLD);
	//printDoubles(train_data, rowCount*_dimensions, rowCount);
	double time_sec_fill = 0;
	double time_one_train = 0;
	double time_reduce_local = 0;
	double time_reduce_all = 0;

	for(int epoch = 0; epoch < epochs; epoch++) {

		MPI_Barrier(MPI_COMM_WORLD);

		//Filling localMap in rank 0 to broadcast to all processes
		auto start3 = std::chrono::high_resolution_clock::now();	
		if (current_rank == 0){
			for (int i = 0; i <_width; i++){
				for (int j = 0; j < _height; j++){
					for (int d = 0; d < _dimensions; d++){
						local_map[calcIndex(i,j,d)] = _weights[calcIndex(i,j,d)];
					}
				}
			}
		}

		MPI_Bcast(local_map, _width*_height*_dimensions, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		auto stop3 = std::chrono::high_resolution_clock::now();
		auto duration3 = std::chrono::duration_cast<std::chrono::duration<double>>(stop3 - start3);
		time_sec_fill += duration3.count();
		MPI_Barrier(MPI_COMM_WORLD);
		
		auto start4 = std::chrono::high_resolution_clock::now();
		train_one_epoch(local_map, train_data, local_numerators, local_denominators, read_count, initial_map_radius, epoch, time_constant);	
	
		auto stop4 = std::chrono::high_resolution_clock::now();
		auto duration4 = std::chrono::duration_cast<std::chrono::duration<double>>(stop4 - start4);
		time_one_train += duration4.count();
		MPI_Barrier(MPI_COMM_WORLD);

		auto start5 = std::chrono::high_resolution_clock::now();
		MPI_Reduce(local_numerators, global_numerators, _width *_height * _dimensions, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		MPI_Reduce(local_denominators, global_denominator, _width * _height, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
		auto stop5 = std::chrono::high_resolution_clock::now();
		auto duration5 = std::chrono::duration_cast<std::chrono::duration<double>>(stop5 - start5);
		time_reduce_local += duration5.count();

		MPI_Barrier(MPI_COMM_WORLD);


		auto start6 = std::chrono::high_resolution_clock::now();
		if (current_rank == 0)
		{
			// Update codebook
			#ifdef ENABLE_OPENMP
			#pragma omp parallel for
			#endif
			for (int i = 0; i < _width * _height; i++) {
				for (int d = 0; d < _dimensions; d++) {
					this->_weights[i*_dimensions + d] = global_numerators[i*_dimensions + d]/global_denominator[i];
				}
			}
		}
		auto stop6 = std::chrono::high_resolution_clock::now();
		auto duration6 = std::chrono::duration_cast<std::chrono::duration<double>>(stop6 - start6);
		time_reduce_all += duration6.count();
	}
	std::cout << "current rank" << current_rank << " time_fill_map " << time_sec_fill << std::endl;
	std::cout << "current rank" << current_rank << " time_one_epoch " << time_one_train << std::endl;
	std::cout << "current rank" << current_rank << " time_reduce_nums_denoms " << time_reduce_local << std::endl;
	std::cout << "current rank" << current_rank << " time_calc_new_map " << time_reduce_all << std::endl;


	if(current_rank == 0)
	{
		free(global_denominator);
		free(global_numerators);
	}
	free(local_map);
	free(local_numerators);
	free(local_denominators);
}

void DSOM::printDoubles(double *doubleList, unsigned int numDoubles, unsigned int numLines)
{
	unsigned int numPerLine = numDoubles/numLines;
	unsigned int counter = 0;
	while(counter < numDoubles)
	{
		for (int j = 0; j< numPerLine; j++)
		{
			std::cout << doubleList[counter] << " ";
			counter++;
		}
		std::cout << std::endl;
	}
}
/*
	Save the width and height of the SOM followed by the weights for each node with a different node's weights on every line
*/
void DSOM::save_weights(std::ostream &out)
{
	out << this->_width << " " << this->_height << std::endl;
	for (int i = 0; i < this->_width; i++)
	{
		for (int j = 0; j < this->_height; j++)
		{
			for (int k = 0; k < this->_dimensions; k++) {
				if (k != 0) {
					out << " ";
				}
				out << this->_weights[calcIndex(i,j,k)];
			}
			out << std::endl;
		}
	}
}

/*
	Load a trained SOM that was saved using the same algorithm as save_weights from an input stream
*/
void DSOM::load_weights(std::istream &in)
{
	// Load SOM dimensions first
	in >> this->_width >> this->_height;

	// Read first line of matrix to get the dimensionality of weights
	this->_dimensions = 0;
	std::string line;
	std::getline(in, line);
	std::getline(in, line);
	std::stringstream ss(line);
	std::vector<double> line1;
	double temp;
	while (ss >> temp) {
		this->_dimensions++;
		line1.push_back(temp);
	}

	// Put first line of matrix into an array in the 3d weights array
	this->_weights = new double[_width * _height * _dimensions];
	for (int k = 0; k < this->_dimensions; k++) {
		_weights[calcIndex(0,0,_dimensions - k - 1)] = line1.back();
		line1.pop_back();
	}

	// Read the rest of the 3d array in
	for (int i = 0; i < this->_width; i++) {
		for (int j = (i == 0 ? 1 : 0); j < this->_height; j++) {
			for (int k = 0; k < _dimensions; k++) {
				in >> this->_weights[calcIndex(i,j,k)];
			}
		}
	}
}

/*
	Normalizes given data to be between 0 and 1 for each feature
*/
void DSOM::normalizeData(double *trainData, int num_examples, double* max, double* min)
{
	for(int i = 0; i < num_examples; i++){
		int rowMod = (num_examples-i-1)*this->_dimensions;
		for(int j = 0; j < this->_dimensions; j++){
			trainData[rowMod+j] = (trainData[rowMod+j] - min[j])/(max[j]-min[j]);
		}
	}


}

/*
	Update a node's weights to better match a given example
*/
void DSOM::updateNodeWeights(int x, int y, double* example, double learning_rate, double influence) {
	for (int d = 0; d < this->_dimensions; d++)
	{
		this->_weights[calcIndex(x,y,d)] += influence * learning_rate * (example[d] - this->_weights[calcIndex(x,y,d)]);
	}
}

/*
	Generate a vector of size numFeatures
*/
double DSOM::randWeight()
{
	return (double)rand() / (RAND_MAX);
}

int DSOM::calcIndex(int x, int y, int d) {
	return (x*_height + y)*_dimensions + d;
}

/*
	Calculates the euclidean distance between two vectors
*/
double DSOM::EucDist(double* v1, double* v2) {
	double total = 0.0;
	for (int i = 0; i < this->_dimensions; i++) {
		total += (v1[i] - v2[i])*(v1[i] - v2[i]);
	}
	return sqrt(total);
}

void DSOM::SqDists(double* m, int loop, int dim, double* output) {
	#pragma omp for
	for (int i = 0; i < loop; i++) {
		output[i] = 0;
		for (int d = 0; d < dim; d++) {
			output[i] += m[i * dim + d] * m[i * dim + d]; 
		}
	}
}

double DSOM::h(int j, int i, double initial_radius, double radius, int* BMUs) {
	int i_y = i % _height;
	int i_x = (i - i_y) / _height;

	// Get BMU coord
	int j_y = BMUs[j] % _height;
	int j_x = (BMUs[j] - j_y) / _height;

	return initial_radius * exp(-(double)((j_x - i_x) * (j_x - i_x) + (j_y - i_y) * (j_y - i_y))/(radius * radius));
}