%{
#include "assembly/surface_normal_dependent_functor.hpp"
#include "common/scalar_traits.hpp"
#include <armadillo>
%}


%typemap(in) PyObject *pyfunc {
    if (!PyCallable_Check($input)) {
        PyErr_SetString(PyExc_TypeError, "Callable object expected!");
        return NULL;
    }
    $1 = $input;
}

%inline %{

namespace Bempp
{

template <typename ValueType_>
class PythonSurfaceNormalDependentFunctor
{
public:
    typedef ValueType_ ValueType;
    typedef typename ScalarTraits<ValueType>::RealType CoordinateType;

    PythonSurfaceNormalDependentFunctor(
        PyObject *pyFunc, int argumentDimension, int resultDimension) :
            m_pyFunc(pyFunc), m_argumentDimension(argumentDimension),
            m_resultDimension(resultDimension) {
        if (!PyCallable_Check(pyFunc))
            PyErr_SetString(PyExc_TypeError, "Python object is not callable");
        Py_INCREF(m_pyFunc); // Increase shared pointer reference count
    }

    ~PythonSurfaceNormalDependentFunctor() {
        Py_DECREF(m_pyFunc);
    }

    int argumentDimension() const {
        return m_argumentDimension;
    }

    int resultDimension() const {
        return m_resultDimension;
    }

    void evaluate(const arma::Col<CoordinateType>& point,
                  const arma::Col<CoordinateType>& normal,
          arma::Col<ValueType>& result_) const
    {
        const int coordinateNumpyType = PythonScalarTraits<CoordinateType>::numpyType;
        const int valueNumpyType = PythonScalarTraits<ValueType>::numpyType;

        // Create the input array
        npy_intp dims1[1];
        dims1[0] = point.n_rows;
        PyObject* pyPoint = PyArray_ZEROS(1, dims1, coordinateNumpyType, NPY_FORTRAN);
        if (!pyPoint)
            throw std::runtime_error("Point array creation failed");
        CoordinateType* pdata = (CoordinateType*)array_data(pyPoint);
        for (size_t i = 0; i < dims1[0]; i++)
            pdata[i] = point(i);

        npy_intp dims2[1];
        dims2[0] = normal.n_rows;
        PyObject* pyNormal = PyArray_ZEROS(1, dims2, coordinateNumpyType, NPY_FORTRAN);
        CoordinateType* ndata = (CoordinateType*)array_data(pyNormal);
        for (size_t i = 0; i<dims2[0]; i++)
            ndata[i] = normal(i);

        // Call into Python
        PyObject* pyReturnVal = PyObject_CallFunctionObjArgs(
            m_pyFunc, pyPoint, pyNormal, NULL);
        if (!pyReturnVal) {
            Py_XDECREF(pyPoint);
            Py_XDECREF(pyNormal);
            throw std::runtime_error("Callable did not execute successfully");
        }
        PyObject* pyReturnValArray = PyArray_FROM_OT(pyReturnVal, valueNumpyType);
        if (!pyReturnValArray) {
            Py_XDECREF(pyPoint);
            Py_XDECREF(pyNormal);
            Py_XDECREF(pyReturnVal);
            throw std::runtime_error("Result from callable cannot be converted to array.");
        }
        int is_new_object;
        PyArrayObject* pyReturnValArrayCont =
            obj_to_array_contiguous_allow_conversion(
                pyReturnValArray, valueNumpyType, &is_new_object);

        // Check size of array
        int asize;
        if (PyArray_CheckScalar(pyReturnValArrayCont)) {
            asize = 1;
        }
        else {
            // Check number of dimensions
            if (array_numdims(pyReturnValArrayCont)!=1) {
                Py_XDECREF(pyPoint);
                Py_XDECREF(pyNormal);
                Py_XDECREF(pyReturnVal);
                Py_XDECREF(pyReturnValArray);
                if (is_new_object)
                    Py_XDECREF(pyReturnValArrayCont);
                throw std::runtime_error("Return array has wrong dimensions!");
            }
            asize = array_size(pyReturnValArrayCont,0);
        }
        if (asize != m_resultDimension) {
            Py_XDECREF(pyNormal);
            Py_XDECREF(pyPoint);
            Py_XDECREF(pyReturnVal);
            Py_XDECREF(pyReturnValArray);
            if (is_new_object)
                Py_XDECREF(pyReturnValArrayCont);
            throw std::runtime_error("Return array has wrong dimensions");
        }

        // Copy data back
        ValueType* data = (ValueType*) array_data(pyReturnValArrayCont);
        for (size_t i = 0; i < m_resultDimension; i++)
            result_(i) = data[i];

        // Clean up

        Py_XDECREF(pyPoint);
        Py_XDECREF(pyNormal);
        Py_XDECREF(pyReturnVal);
        Py_XDECREF(pyReturnValArray);
        if (is_new_object)
            Py_XDECREF(pyReturnValArrayCont);
    }

 private:
    PyObject* m_pyFunc;
    int m_argumentDimension;
    int m_resultDimension;
};

} // namespace Bempp

%}

namespace Bempp
{

BEMPP_INSTANTIATE_SYMBOL_TEMPLATED_ON_VALUE(PythonSurfaceNormalDependentFunctor);

} // namespace Bempp

/*BEMPP_DECLARE_PYTHON_SURFACE_NORMAL_DEPENDENT_FUNCTOR(float,float32,NPY_FLOAT)
BEMPP_DECLARE_PYTHON_SURFACE_NORMAL_DEPENDENT_FUNCTOR(double,float64,NPY_DOUBLE)
BEMPP_DECLARE_PYTHON_SURFACE_NORMAL_DEPENDENT_FUNCTOR(std::complex<float>,complex64,NPY_CFLOAT)
BEMPP_DECLARE_PYTHON_SURFACE_NORMAL_DEPENDENT_FUNCTOR(std::complex<double>,complex128,NPY_CDOUBLE)

%pythoncode %{

def surfaceNormalDependentFunctor(fun, valueType='float64',
        argumentDimension=3, resultDimension=1):
    valueType = checkType(valueType)
    return constructObjectTemplatedOnValue(
        "PythonSurfaceNormalDependentFunctor",
        valueType, fun, argumentDimension, resultDimension)

%}
*/
