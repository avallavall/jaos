// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** A second-order cone, or a rotated one: {@code 2 x0 x1 >= ||(x2, ...)||²}. */
public enum ConeType {
    QUADRATIC(1), ROTATED(2);

    final int code;

    ConeType(int code) {
        this.code = code;
    }
}
