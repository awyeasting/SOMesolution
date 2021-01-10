/*
 * This file is part of SOMeSolution.
 *
 * Developed for Pacific Northwest National Laboratory.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the BSD 3-Clause License as published by
 * the Software Package Data Exchange.
 */

#ifndef SOM_H
#define SOM_H

#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <math.h>
#include <omp.h>
#include <sstream>
#include <string>
#include <string.h>
#include <time.h>
#include <vector>

#include "cublas_v2.h"

#define GPU_BASED_CODEBOOK_INIT false

class SOM
{
public:
	SOM(unsigned int width, unsigned int height);
	SOM(std::istream &in);

	void gen_train_data(unsigned int examples, unsigned int dimensions, unsigned int seedValue);
	bool load_train_data(std::string &fileName, bool hasLabelRow, bool hasLabelColumn);
	void destroy_train_data();

	void train_data(unsigned int epochs, unsigned int map_seed, int num_gpus = -1);
	void train_data(unsigned int epochs, unsigned int map_seed, int num_gpus, int gpu_num_offset);
	void train_data(unsigned int epochs, unsigned int map_seed, int num_gpus, int* gpus_assigned);

	void save_weights(std::ostream &out);

	int get_num_gpus();

	std::fstream& GotoLine(std::fstream& file, unsigned int num);
private:

	int _rank;
	int _groupRank;
	int _numProcs;
	int _numGroupProcs;
	int _currentEpoch;
	int _numEpochs;
	int _numGPUs;
	int _totalNodeGPUs;

	int *_gpus;

	unsigned int _width;
	unsigned int _height;
	unsigned int _mapSize;
	unsigned int _numExamples;
	unsigned int _dimensions;
	unsigned int _mapSeed;

	float _initial_map_radius;
	float _neighborhood_radius;
	float _time_constant;
	float* _weights;
	float* _trainData;
	float* _featureMaxes;
	float* _featureMins;

	// CUBLAS handles (per device)
	cublasHandle_t* _handles;
	// Local node's all gpu reduced numerators and denominators
	float *_numer;
	float *_denom;
	// Global all node all gpu reduced numerators and denominators
	float *_global_numer;
	float *_global_denom;
	// GPU copies of training data and weights
	float **_d_train;
	float **_d_weights;
	// GPU copies of numerators and denominators
	float **_d_numer;
	float **_d_denom;
	// CPU copies of GPU numerators and denominators
	float **_gnumer;
	float **_gdenom;
	// Number of examples per gpu
	unsigned int *_GPU_EXAMPLES;
	// Training data offset per gpu
	unsigned int *_GPU_OFFSET;

	void loadWeights(std::istream &in);
	void normalizeData(float *trainData);
	int calcIndex(int x, int y, int d);

	void trainOneEpochOneGPU(int gpu);
	void trainOneEpochMultiGPU();

	void allocNumerDenom();
	void allocGPUTrainMemory();

	void chooseGPUs();
	void initMultiGPUSetup();
	void initGPUTrainData();
	void initCodebook();
	void initCodebookOnCPU();
	void initCodebookOnGPU();

	void updateGPUCodebooks();
	void updateGPUCodebook(int gpu);

	void trainData();

	void freeGPUMemory();

	static float randWeight();
};

#endif