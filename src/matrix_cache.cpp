#include <algorithm>
#include <omp.h>
#include <iostream>

#include "matrix_cache.h"
#include "probability.h"

bool matrix::is_zero() const
{
    return *max_element(values.begin(), values.end()) == 0;
}

matrix_cache::~matrix_cache()
{
    for (auto m : _matrix_cache)
    {
        delete m.second;
    }
}

/* START: Likelihood computation ---------------------- */
//! Calls BD formulas, but checks/populates cache
double matrix_cache::get_from_parent_fam_size_to_c(double lambda, double branch_length, int parent_size, int child_size) const {
    // The probability of 0 remaining 0 is 1
    // The probability of 0 going to any other count is 0 (if you lose the gene family, you do not regain it)
    if (parent_size == 0.0)
        return child_size == 0.0 ? 1.0 : 0.0;

    return the_probability_of_going_from_parent_fam_size_to_c(lambda, branch_length, parent_size, child_size);
}

#define MATRIX_CACHING

//! Compute transition probability matrix for all gene family sizes from 0 to size-1 (=_max_root_family_size-1)
matrix matrix_cache::get_matrix(int size, double branch_length, double lambda) {
#ifndef MATRIX_CACHING
    matrix result(size);
    result.at(0).resize(size);

    double zero_val = 1.0;
    result.at(0)[0] = get_from_parent_fam_size_to_c(lambda, branch_length, 0, 0, &zero_val); // here we set the probability of 0 remaining 0 to 1 (if you lose the gene family, you do not regain it)
    zero_val = 0.0;
    for (int i = 0; i < result[0].size(); ++i)
    {
        result.at(0)[0] = get_from_parent_fam_size_to_c(lambda, branch_length, 0, i, &zero_val);
    }
    for (int s = 1; s < size; s++) {
        result.at(s).resize(size);

        for (int c = 0; c < size; c++) {
            // result[s][c] = the_probability_of_going_from_parent_fam_size_to_c(lambda, branch_length, s, c);
            result.at(s)[c] = get_from_parent_fam_size_to_c(lambda, branch_length, s, c, NULL);
            // cout << "s = " << s << " c= " << c << ", result=" << result[s][c] << endl;
        }

    }

    return result;
#else
    // cout << "Matrix request " << size << "," << branch_length << "," << lambda << endl;

    matrix *result = NULL;
    matrix_cache_key key(size, lambda, branch_length);
    if (_matrix_cache.find(key) != _matrix_cache.end())
    {
        result = _matrix_cache.at(key);
    }

    if (result == NULL)
    {
        ostringstream ost;
        ost << "Failed to find matrix for " << size << "," << branch_length << "," << lambda;
        throw std::runtime_error(ost.str());
    }
    return *result;
#endif
}

vector<double> get_lambda_values(lambda *p_lambda)
{
    vector<double> lambdas;
    auto sl = dynamic_cast<single_lambda *>(p_lambda);
    if (sl)
    {
        lambdas.push_back(sl->get_single_lambda());
    }
    else
    {
        auto ml = dynamic_cast<multiple_lambda *>(p_lambda);
        lambdas = ml->get_lambdas();
    }
    return lambdas;
}

void matrix_cache::precalculate_matrices(int size, const std::vector<double>& lambdas, const std::set<double>& branch_lengths)
{
    // build a list of required matrices
    vector<matrix_cache_key> keys;
    for (double lambda : lambdas)
    {
        for (double branch_length : branch_lengths)
        {
            matrix_cache_key key(size, lambda, branch_length);
            if (_matrix_cache.find(key) == _matrix_cache.end())
            {
                keys.push_back(key);
            }
        }
    }

    // calculate matrices in parallel
    vector<matrix *> matrices(keys.size());
    generate(matrices.begin(), matrices.end(), [size] { return new matrix(size); });

#pragma omp parallel for
    for (size_t i = 0; i<keys.size(); ++i)
    {
        double lambda = keys[i].lambda();
        double branch_length = keys[i].branch_length();

        matrix *m = matrices[i];
        m->set(0, 0, get_from_parent_fam_size_to_c(lambda, branch_length, 0, 0));
        for (int i = 0; i < m->size(); ++i)
        {
            m->set(0, 0, get_from_parent_fam_size_to_c(lambda, branch_length, 0, i));
        }
        for (int s = 1; s < size; s++) {
            for (int c = 0; c < size; c++) {
                m->set(s, c, get_from_parent_fam_size_to_c(lambda, branch_length, s, c));
            }
        }
    }

    // copy matrices to our internal map
    for (size_t i = 0; i < keys.size(); ++i)
    {
        _matrix_cache[keys[i]] = matrices[i];
    }
}


