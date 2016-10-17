/* Function to evaluate F-value in permutest.cca. For instance, in R
 * CMD check in Macbook Air this function uses 1/4 of computing time,
 * and in applications with constrained ordination this is the major
 * function. Even small speed-up in this function will have a
 * considerable impact in running time.
 */


/* Function called form getF to evaluate the sum of all eigenvalues */

double getEV(double *x, int nr, int nc, int isDB)
{
    int i, ii;
    double sumev;
    switch(isDB) {
    case 1:
	for(i = 0, sumev = 0; i < nr; i++) {
	    ii = i * nr + i;
	    sumev += x[ii] * x[ii];
	}
	break;
    case 0:
	for(i = 0, sumev = 0; i < nr * nc; i++)
	    sumev += x[i] * x[i];
	break;
    }
    return sumev;
}

/* do_getF: get the F value. At the first pass *only* for RDA.

 */
#include <R.h>
#include <Rinternals.h>
#include <R_ext/Linpack.h> /* QR */
#include <R_ext/Lapack.h>  /* SVD */

/* LINPACK uses the same function to find fit and/or residuals with
 * the following switches */
#define FIT 1
#define RESID 10

SEXP do_getF(SEXP perms, SEXP E, SEXP QR, SEXP QZ, SEXP isPartial)
{
    int i, j, k, ki,
	nperm = nrows(perms), nr = nrows(E), nc = ncols(E),
	PARTIAL = asInteger(isPartial);
    SEXP ans = PROTECT(allocMatrix(REALSXP, nperm, 2));
    double *rans = REAL(ans);
    SEXP Y = PROTECT(duplicate(E));
    double *rY = REAL(Y);

    /* pointers and new objects to the QR decomposition */

    double *qr = REAL(VECTOR_ELT(QR, 0));
    int qrank = asInteger(VECTOR_ELT(QR, 1));
    double *qraux = REAL(VECTOR_ELT(QR, 2));
    double *Zqr, *Zqraux;
    int Zqrank;
    if (PARTIAL) {
	Zqr = REAL(VECTOR_ELT(QZ, 0));
	Zqrank = asInteger(VECTOR_ELT(QZ, 1));
	Zqraux = REAL(VECTOR_ELT(QZ, 2));
    }

    double *fitted = (double *) R_alloc(nr * nc, sizeof(double));
    double *resid = (double *) R_alloc(nr * nc, sizeof(double));
    double *qrwork = (double *) R_alloc(nr, sizeof(double));
    double dummy;
    int info, qrkind;

    /* double *wtake = (double *) R_alloc(nr, sizeof(double)); */

    /* permutation matrix must be duplicated */
    SEXP dperms = PROTECT(duplicate(perms));
    int *iperm = INTEGER(dperms);

    /* permutations to zero base */
    for(i = 0; i < nperm * nr; i++)
	iperm[i]--;

    /* loop over rows of permutation matrix */
    for (k = 0; k < nperm; k++) {
	/* Y will be permuted data */
	for (i = 0; i < nr; i++) {
	    ki = iperm[k + nperm * i];
	    for(j = 0; j < nc; j++) {
		rY[i + nr * j] = REAL(E)[ki + nr * j];
	    }
	}

	/* Partial model: qr.resid(QZ, Y) with LINPACK */
	if (PARTIAL) {
	    qrkind = RESID;
	    for(i = 0; i < nc; i++)
		F77_CALL(dqrsl)(Zqr, &nr, &nr, &Zqrank, Zqraux, rY + i*nr,
				&dummy, qrwork, &dummy, rY + i*nr, &dummy,
				&qrkind, &info);
	}

	/* qr.fitted(QR, Y) + qr.resid(QR, Y) with LINPACK */
	if (PARTIAL)
	    qrkind = FIT + RESID;
	else
	    qrkind = FIT;
	for (i = 0; i < nc; i++)
	    F77_CALL(dqrsl)(qr, &nr, &nr, &qrank, qraux, rY + i*nr, &dummy,
			    qrwork, &dummy, resid + i*nr, fitted + i*nr,
			    &qrkind, &info);

	/* Eigenvalues: only sum of all, first ev not yet
	 * implemented. If the sum of all eigenvalues does not change,
	 * we have only ev of CCA component. */

	rans[k] = getEV(fitted, nr, nc, 0);
	if (PARTIAL)
	    rans[k + nperm] = getEV(resid, nr, nc, 0);

    } /* end permutation loop */

    UNPROTECT(3);
    return ans;
}