// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.util.ArrayList;
import java.util.List;

/** A linear expression, built term by term: {@code new Expr().add(2, x).add(y).add(3)}. */
public final class Expr {
    final List<Integer> cols = new ArrayList<>();
    final List<Double> coefs = new ArrayList<>();
    double constant;

    public static Expr of(Var v) {
        return new Expr().add(v);
    }

    public Expr add(double coef, Var v) {
        cols.add(v.index);
        coefs.add(coef);
        return this;
    }

    public Expr add(Var v) {
        return add(1.0, v);
    }

    public Expr add(double c) {
        constant += c;
        return this;
    }

    /** Adds {@code coef} times every term and the constant of {@code e}. */
    public Expr add(double coef, Expr e) {
        int n = e.cols.size();
        for (int k = 0; k < n; k++) {
            cols.add(e.cols.get(k));
            coefs.add(coef * e.coefs.get(k));
        }
        constant += coef * e.constant;
        return this;
    }

    /** Adds every term and the constant of {@code e}. */
    public Expr add(Expr e) {
        return add(1.0, e);
    }

    Expr copy() {
        return new Expr().add(this);
    }
}
