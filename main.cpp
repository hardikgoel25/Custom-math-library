#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <iomanip>
#include "Hardik_math.h"
using namespace std;

double absolute_error(double true_value, double approx_value)
{
    return fabs(true_value - approx_value);
}

double relative_error(double true_value, double approx_value)
{
    if (fabs(true_value) < 1e-15)
    {
        return NAN;
    }
    return fabs(true_value - approx_value) / fabs(true_value);
}

int main()
{
    double values[] = {0, 0.1, HRD_PI / 6, 0.3, HRD_PI / 2, HRD_PI, 1.5 * HRD_PI, 1, -HRD_INFINITE};
    int num_values = sizeof(values) / sizeof(values[0]);

    cout << fixed << setprecision(15);
    cout << setw(15) << "x"
         << setw(25) << "Calculated Value"
         << setw(25) << "True Value"
         << setw(25) << "Absolute Error"
         << setw(25) << "Relative Error"
         << endl;

    cout << string(115, '-') << endl;

    // exp(x)
    cout << "exp(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = exp(x);
        long double calculated = Exp(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // cos(x)
    cout << "cos(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = cos(x);
        long double calculated = Cosine(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // sin(x)
    cout << endl
         << "sin(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = sin(x);
        long double calculated = Sine(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // tan(x)
    cout << endl
         << "tan(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        if (Cosine(x) == 0)
        {
            cout << setw(15) << x << setw(25) << "INFINITY" << setw(25) << "INFINITY"
                 << setw(25) << "N/A" << setw(25) << "N/A" << endl;
            continue;
        }
        long double trueVal = tan(x);
        long double calculated = Tangent(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // cosec(x)
    cout << endl
         << "cosec(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        if (Sine(x) == 0)
        {
            cout << setw(15) << x << setw(25) << "INFINITY" << setw(25) << "INFINITY"
                 << setw(25) << "N/A" << setw(25) << "N/A" << endl;
            continue;
        }
        long double trueVal = 1 / sin(x);
        long double calculated = Cosecant(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // sec(x)
    cout << endl
         << "sec(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        if (Cosine(x) == 0)
        {
            cout << setw(15) << x << setw(25) << "INFINITY" << setw(25) << "INFINITY"
                 << setw(25) << "N/A" << setw(25) << "N/A" << endl;
            continue;
        }
        long double trueVal = 1 / cos(x);
        long double calculated = Secant(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // cot(x)
    cout << endl
         << "cot(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        if (Tangent(x) == 0)
        {
            cout << setw(15) << x << setw(25) << "INFINITY" << setw(25) << "INFINITY"
                 << setw(25) << "N/A" << setw(25) << "N/A" << endl;
            continue;
        }
        long double trueVal = 1 / tan(x);
        long double calculated = Cotangent(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // ln(x)
    cout << "ln(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = log(x);
        long double calculated = Ln(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // Log(x)
    cout << "Log(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = log10(x);
        long double calculated = Log(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // Lg(x)
    cout << "Lg(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = log2(x);
        long double calculated = Lg(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arctan(x)
    cout << "arctan(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = atan(x);
        long double calculated = arcTangent(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // arcsin(x)
    cout << "arcsin(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = asinl(x);
        long double calculated = arcSine(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // arccos(x)
    cout << "arccos(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = acosl(x);
        long double calculated = arcCosine(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arccosec(x)
    cout
        << "arccosec(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = atan(1.0 / sqrt(x * x - 1));
        long double calculated = arcCosecant(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arcsec(x)
    cout
        << "arcsec(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = atan(Sqrt((x * x) - 1));
        long double calculated = arcSecant(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arccot(x)
    cout
        << "arccot(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = atan(1 / x);
        long double calculated = arcCotangent(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // sinh(x)
    cout << "sinh(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = sinh(x);
        long double calculated = Sinh(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // cosh(x)
    cout << "cosh(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = cosh(x);
        long double calculated = Cosh(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // tanh(x)
    cout << "tanh(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = tanh(x);
        long double calculated = Tanh(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // coth(x)
    cout << "coth(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = 1 / tanh(x);
        long double calculated = Coth(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    // sech(x)
    cout << "sech(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = 1 / cosh(x);
        long double calculated = Sech(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // cosech(x)
    cout << "cosech(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = 1 / sinh(x);
        long double calculated = Csch(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arcSinh(x)
    cout
        << "arcsinh(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = asinhl(x);
        long double calculated = arcSinh(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arcCosh(x)
    cout
        << "arccosh(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = acoshl(x);
        long double calculated = arcCosh(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arctanh(x)
    cout
        << "arctanh(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = atanhl(x);
        long double calculated = arcTanh(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arccsch(x)
    cout
        << "arccsch(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = asinhl(1 / x);
        long double calculated = arcCsch(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arcsech(x)
    cout
        << "arcsech(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = acoshl(1 / x);
        long double calculated = arcSech(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }
    // arccoth(x)
    cout
        << "arccoth(x)" << endl;
    for (int i = 0; i < num_values; ++i)
    {
        double x = values[i];
        long double trueVal = atanhl(1 / x);
        long double calculated = arcCoth(x);
        double absError = absolute_error(trueVal, calculated);
        double relError = relative_error(trueVal, calculated);

        cout << setw(15) << x
             << setw(25) << calculated
             << setw(25) << trueVal
             << setw(25) << absError
             << setw(25) << relError
             << endl;
    }

    return 0;
}
