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
}
