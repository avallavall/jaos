// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** One status per column and one per row. */
public record Basis(BasisStatus[] colStatus, BasisStatus[] rowStatus) {}
