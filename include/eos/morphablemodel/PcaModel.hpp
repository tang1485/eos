/*
 * Eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: include/eos/morphablemodel/PcaModel.hpp
 *
 * Copyright 2014, 2015 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef PCAMODEL_HPP_
#define PCAMODEL_HPP_

#include "opencv2/core/core.hpp"

#include <string>
#include <vector>
#include <array>
#include <random>

namespace eos {
	namespace morphablemodel {

/**
 * Forward declarations of free functions
 */
cv::Mat normalisePcaBasis(cv::Mat unnormalisedBasis, cv::Mat eigenvalues);
cv::Mat unnormalisePcaBasis(cv::Mat normalisedBasis, cv::Mat eigenvalues);

/**
 * This class represents a PCA-model that consists of:
 *   - a mean vector (y x z)
 *   - a PCA basis matrix (unnormalised and normalised)
 *   - a PCA variance vector.
 *
 * It also contains a list of triangles to built a mesh as well as a mapping
 * from landmark points to the corresponding vertex-id in the mesh.
 * It is able to return instances of the model as meshes.
 */
class PcaModel
{
public:
	
	/**
	 * Construct a PCA model from given mean, normalised PCA basis, eigenvalues
	 * and triangle list.
	 *
	 * See the documentation of the member variables for how the data should
	 * be arranged.
	 *
	 * @param[in] mean The mean used to build the PCA model.
	 * @param[in] pcaBasis The PCA basis (eigenvectors), normalised (multiplied by the eigenvalues).
	 * @param[in] eigenvalues The eigenvalues used to build the PCA model.
	 * @param[in] triangleList An index list of how to assemble the mesh.
	 */
	PcaModel(cv::Mat mean, cv::Mat pcaBasis, cv::Mat eigenvalues, std::vector<std::array<int, 3>> triangleList) : mean(mean), normalisedPcaBasis(pcaBasis), eigenvalues(eigenvalues), triangleList(triangleList)
	{
		const auto seed = std::random_device()();
		engine.seed(seed);
		unnormalisedPcaBasis = unnormalisePcaBasis(normalisedPcaBasis, eigenvalues);
	};

	/**
	 * Returns the number of principal components in the model.
	 *
	 * @return The number of principal components in the model.
	 */
	int getNumberOfPrincipalComponents() const
	{
		// Note: we could assert(normalisedPcaBasis.cols==unnormalisedPcaBasis.cols)
		return normalisedPcaBasis.cols;
	};

	/**
	 * Returns the dimension of the data, i.e. the number of shape dimensions.
	 *
	 * As the data is arranged in a [x y z x y z ...] fashion, dividing this by
	 * three yields the number of vertices in the model.
	 *
	 * @return The dimension of the data.
	 */
	int getDataDimension() const
	{
		// Note: we could assert(normalisedPcaBasis.rows==unnormalisedPcaBasis.rows)
		return normalisedPcaBasis.rows;
	};

	/**
	 * Returns a list of triangles on how to assemble the vertices into a mesh.
	 *
	 * @return The list of triangles to build a mesh.
	 */
	std::vector<std::array<int, 3>> getTriangleList() const
	{
		return triangleList;
	};

	/**
	 * Returns the mean of the model.
	 *
	 * @return The mean of the model.
	 */
	cv::Mat getMean() const
	{
		return mean;
	};

	/**
	 * Return the value of the mean at a given vertex id.
	 *
	 * @param[in] vertexIndex A vertex id.
	 * @return A homogeneous vector containing the values at the given vertex id.
	 */
	cv::Vec4f getMeanAtPoint(int vertexIndex) const
	{
		vertexIndex *= 3;
		if (vertexIndex >= mean.rows) {
			throw std::out_of_range("The given vertex id is larger than the dimension of the mean.");
		}
		return cv::Vec4f(mean.at<float>(vertexIndex), mean.at<float>(vertexIndex + 1), mean.at<float>(vertexIndex + 2), 1.0f);
	};

	/**
	 * Draws a random sample from the model, where the coefficients are drawn
	 * from a standard normal (or with the given standard deviation).
	 *
	 * @param[in] sigma The standard deviation.
	 * @return A random sample from the model.
	 */
	cv::Mat drawSample(float sigma = 1.0f)
	{
		std::normal_distribution<float> distribution(0.0f, sigma); // this constructor takes the stddev

		std::vector<float> alphas(getNumberOfPrincipalComponents());

		for (auto&& a : alphas) {
			a = distribution(engine);
		}

		return drawSample(alphas);
	};

	/**
	 * Returns a sample from the model with the given PCA coefficients.
	 * The given coefficients should follow a standard normal distribution, i.e.
	 * not be "normalised" with their eigenvalues/variances.
	 *
	 * @param[in] coefficients The PCA coefficients used to generate the sample.
	 * @return A model instance with given coefficients.
	 */
	cv::Mat drawSample(std::vector<float> coefficients)
	{
		// Fill the rest with zeros if not all coefficients are given:
		if (coefficients.size() < getNumberOfPrincipalComponents()) {
			coefficients.resize(getNumberOfPrincipalComponents());
		}
		cv::Mat alphas(coefficients);

		cv::Mat modelSample = mean + normalisedPcaBasis * alphas;

		return modelSample;
	};

	/**
	 * Returns the PCA basis matrix, i.e. the eigenvectors.
	 * Each column of the matrix is an eigenvector.
	 * The returned basis is normalised, i.e. every eigenvector
	 * is normalised by multiplying it with the square root of its eigenvalue.
	 * 
	 * Returns a clone of the matrix so that the original cannot
	 * be modified. TODO: No, don't return a clone.
	 *
	 * @return Returns the normalised PCA basis matrix.
	 */
	cv::Mat getNormalisedPcaBasis() const
	{
		return normalisedPcaBasis.clone();
	};
	
	/**
	 * Returns the PCA basis for a particular vertex.
	 * The returned basis is normalised, i.e. every eigenvector
	 * is normalised by multiplying it with the square root of its eigenvalue.
	 *
	 * @param[in] vertexId A vertex index. Make sure it is valid.
	 * @return A Mat that points to the rows in the original basis.
	 */
	cv::Mat getNormalisedPcaBasis(int vertexId) const
	{
		vertexId *= 3; // the basis is stored in the format [x y z x y z ...]
		return normalisedPcaBasis.rowRange(vertexId, vertexId + 3);
	};

	/**
	 * Returns the PCA basis matrix, i.e. the eigenvectors.
	 * Each column of the matrix is an eigenvector.
	 * The returned basis is unnormalised, i.e. not scaled by their eigenvalues.
	 *
	 * Returns a clone of the matrix so that the original cannot
	 * be modified. TODO: No, don't return a clone.
	 *
	 * @return Returns the unnormalised PCA basis matrix.
	 */
	cv::Mat getUnnormalisedPcaBasis() const
	{
		return unnormalisedPcaBasis.clone();
	};

	/**
	 * Returns the PCA basis for a particular vertex.
	 * The returned basis is unnormalised, i.e. not scaled by their eigenvalues.
	 *
	 * @param[in] vertexId A vertex index. Make sure it is valid.
	 * @return A Mat that points to the rows in the original basis.
	 */
	cv::Mat getUnnormalisedPcaBasis(int vertexId) const
	{
		vertexId *= 3; // the basis is stored in the format [x y z x y z ...]
		return unnormalisedPcaBasis.rowRange(vertexId, vertexId + 3);
	};

	/**
	 * Returns an eigenvalue.
	 *
	 * @param[in] index The index of the eigenvalue to return.
	 * @return The eigenvalue.
	 */
	float getEigenvalue(int index) const
	{
		return eigenvalues.at<float>(index);
	};

private:
	std::mt19937 engine; ///< Random number engine used to draw random coefficients.
	
	cv::Mat mean; ///< A 3m x 1 col-vector (xyzxyz...)', where m is the number of model-vertices.
	cv::Mat normalisedPcaBasis; ///< The normalised PCA basis matrix. m x n (rows x cols) = numShapeDims x numShapePcaCoeffs, (=eigenvector matrix V). Each column is an eigenvector.
	cv::Mat unnormalisedPcaBasis; ///< The unnormalised PCA basis matrix. m x n (rows x cols) = numShapeDims x numShapePcaCoeffs, (=eigenvector matrix V). Each column is an eigenvector.
	cv::Mat eigenvalues; ///< A col-vector of the eigenvalues (variances in the PCA space).

	std::vector<std::array<int, 3>> triangleList; ///< List of triangles that make up the mesh of the model.
};


/**
 * Takes an unnormalised PCA basis matrix (a matrix consisting
 * of the eigenvectors and normalises it, i.e. multiplies each
 * eigenvector by the square root of its corresponding
 * eigenvalue.
 *
 * @param[in] unnormalisedBasis An unnormalised PCA basis matrix.
 * @param[in] eigenvalues A row or column vector of eigenvalues.
 * @return The normalised PCA basis matrix.
 */
inline cv::Mat normalisePcaBasis(cv::Mat unnormalisedBasis, cv::Mat eigenvalues)
{
	using cv::Mat;
	Mat normalisedPcaBasis(unnormalisedBasis.size(), unnormalisedBasis.type()); // empty matrix with the same dimensions
	Mat sqrtOfEigenvalues = eigenvalues.clone();
	for (int i = 0; i < eigenvalues.rows; ++i) {
		sqrtOfEigenvalues.at<float>(i) = std::sqrt(eigenvalues.at<float>(i));
	}
	// Normalise the basis: We multiply each eigenvector (i.e. each column) with the square root of its corresponding eigenvalue
	for (int basis = 0; basis < unnormalisedBasis.cols; ++basis) {
		Mat normalisedEigenvector = unnormalisedBasis.col(basis).mul(sqrtOfEigenvalues.at<float>(basis));
		normalisedEigenvector.copyTo(normalisedPcaBasis.col(basis));
	}

	return normalisedPcaBasis;
};

/**
 * Takes a normalised PCA basis matrix (a matrix consisting
 * of the eigenvectors and denormalises it, i.e. multiplies each
 * eigenvector by 1 over the square root of its corresponding
 * eigenvalue.
 *
 * @param[in] normalisedBasis A normalised PCA basis matrix.
 * @param[in] eigenvalues A row or column vector of eigenvalues.
 * @return The unnormalised PCA basis matrix.
 */
inline cv::Mat unnormalisePcaBasis(cv::Mat normalisedBasis, cv::Mat eigenvalues)
{
	using cv::Mat;
	Mat unnormalisedBasis(normalisedBasis.size(), normalisedBasis.type()); // empty matrix with the same dimensions
	Mat oneOverSqrtOfEigenvalues = eigenvalues.clone();
	for (int i = 0; i < eigenvalues.rows; ++i) {
		oneOverSqrtOfEigenvalues.at<float>(i) = 1.0f / std::sqrt(eigenvalues.at<float>(i));
	}
	// De-normalise the basis: We multiply each eigenvector (i.e. each column) with 1 over the square root of its corresponding eigenvalue
	for (int basis = 0; basis < normalisedBasis.cols; ++basis) {
		Mat unnormalisedEigenvector = normalisedBasis.col(basis).mul(oneOverSqrtOfEigenvalues.at<float>(basis));
		unnormalisedEigenvector.copyTo(unnormalisedBasis.col(basis));
	}

	return unnormalisedBasis;
};

	} /* namespace morphablemodel */
} /* namespace eos */

#endif /* PCAMODEL_HPP_ */