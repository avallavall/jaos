# Review 02-03: las 19 rechazadas, tres sitios del postsolve

Todo verificado ejecutando el replay sobre las instancias reales, no por
lectura. Rutas con barra normal para que el fichero sea copiable.

## F1 - rest se lee antes de que el acumulador este lleno (16 de 19)

src/presolve.c:1540, dentro de JM_PS_SINGLETON_COL:

    const double rest = orig->sol_row[i];
    const double rl = orig->row_lower[i], ru = orig->row_upper[i];

orig->sol_row[i] no esta terminado en ese momento. jm_postsolve_expand lo
siembra con la actividad del reduced solve (linea 1852) y cada columna
eliminada suma su parte cuando su propio registro se reproduce, en LIFO
estricto (linea 1891). Un registro con indice de arena MENOR se empujo
ANTES y por tanto se reproduce DESPUES. Toda columna de la fila i que
presolve quito antes que esta singleton column falta en rest.

La recuperacion apunta entonces a los bounds ORIGINALES de la fila (linea
1541) contra esa suma parcial, y x_j absorbe entero lo que las columnas
pendientes van a sumar luego.

### Medicion

En ken-07, viol - F = 0 EXACTO en las seis filas violadas (2402, 2405,
2413, 2417, 2419, 2422), donde F es la suma de lo que aportan a esa fila
las columnas cuyos registros se reproducen despues.

Traza de la fila 2413 (rl = ru = 1506):

    r=1482 SINGLETON_COL idx=2413 idx2=3589 coef=1 reclo=0 rechi=10195
           x: ->1506   sol_row[2413]: 0 -> 1506
    r=1322 FIXED_COL     idx=2397 val=18     sol_row[2413]: 1506 -> 1524
    ... 53 registros FIXED_COL mas (columnas 2205..2397, coeficiente 1) ...
    r=1274 FIXED_COL     idx=2205 val=14     sol_row[2413]: 2245 -> 2259

La columna 3589 se publica en 1506 porque rest valia 0. El valor correcto
era 1506 - 753 = 753.

### La division que da 1/3

Con R el RHS de la igualdad y F el total de las columnas que se reproducen
despues: actividad publicada = R + F, violacion = F, traffic = R + F
(todos los terminos positivos aqui). Entonces rowrel = F/(R+F).

En las seis filas de ken-07 F es exactamente la mitad de R:
753/1506, 707/1414, 742/1484, 706/1412, 729/1458. Sustituyendo:
(R/2)/(3R/2) = 1/3 exacto.

El medio es una propiedad de la familia ken, donde los arcos fijados llevan
justo la mitad de cada fila de demanda. Lo que aporta el codigo es que la
violacion ES F, exactamente.

### Segunda forma del mismo defecto (tuff, y por que ahi viol no es F)

Cuando el extremo que pide la fila cae por debajo del bound propio de la
columna, want_lo = max(rec->lo, lo_j) recorta en rec->lo y x_j sale en su
propio bound.

tuff, fila 295 (rl = ru = 0):

    r=96 SINGLETON_COL idx=295 idx2=565 coef=-1 reclo=0 rechi=inf
         rest = -122.94 ; lo_j = (0 - (-122.94))/(-1) = -122.94
         want_lo = max(0, -122.94) = 0  ->  x_565 = 0
    r=93 FIXED_COL     idx=553 val=1100.41  sol_row[295]: -122.94 -> 977.47

Actividad final 977.47 contra un bound de 0. El valor correcto, 977.47,
estaba dentro del rango de la columna.

### Instancias

Grupo A: czprob, share1b, tuff, ken-07, ken-11, ken-13, ken-18.
Mitades de fila del grupo C: 25fv47, finnis, lotfi, perold, pilot-we,
vtp-base. Grupo D: pilot-ja, pilotnov, pilot87.
Coincide con el barrido: todo residuo de fila lo limpia PS_NO_SINGLETON_COL.

### Contraste que confirma el diagnostico

JM_PS_FREE_COL_SINGLETON graba los bounds YA DESPLAZADOS de la fila en el
momento de disparar (linea 830, .lo = cur_rl[i], .hi = cur_ru[i]) y por eso
sale bien: coef*x_j + suma(a*v) = rec->lo + suma(a*v) = rl_orig. El barrido
exonera a FCS. La familia que guarda el bound desplazado esta limpia; la
que lo vuelve a derivar en tiempo de replay desde los bounds originales, no.

### El assert no lo coge

assert(want_lo <= want_hi) (linea 1559) se cumple en ambos casos medidos,
porque el objetivo equivocado es internamente consistente. Por eso make
test y make sanitize pasan.

## F2 - zero_works supone un productor que puede no haber sido el ultimo (7 de 19)

src/presolve.c:1477-1499, dentro de JM_PS_SINGLETON_ROW:

    const double d0 = orig->sol_redcost[j];
    const double v0 = orig->sol_col[j];
    const bool zero_works = d0 == 0.0 ||
        (d0 > 0.0 && v0 == orig->col_lower[j]) ||
        (d0 < 0.0 && v0 == orig->col_upper[j]);

El test trata ese par como si fuera el coste reducido dual-factible del
reduced solve para una columna cuyo unico apriete de bound vino de ESTA
fila. Tres productores distintos pueden haber escrito esos slots antes, y
bajo cada uno el test es falso.

### (a) Otro registro JM_PS_SINGLETON_ROW sobre la misma columna

El que creo el bound activo. Medido:

  instancia | se lleva y      | tl/th | responsable          | tl/th | y publicado
  ----------|-----------------|-------|----------------------|-------|------------
  bnl1      | 75 (coef -7)    | 0/0   | 4 (coef -2.1978)     | 0/1   | +0.22905
  bnl2      | 13 (coef -.2198)| 0/0   | 6 (coef -2.442)      | 0/1   | +9.81024
  e226      | 14 (coef 1)     | 0/0   | 13 (coef 0.92)       | 0/1   | -1.16452
  25fv47    | 696 (coef -1)   | 0/0   | 695 (coef 1, [17,17])| 1/1   | -6.12218

En bnl1 el bound superior reducido de la columna 69 es 0.7735007735, que es
exactamente -1.7/-2.1978, el implied_hi de la fila 4. La fila 4 se queda con
y = 0 y la 75 se lleva d_red/coef = -1.6034/-7 = +0.22905. La fila 75 tiene
bounds [-inf, 0] y actividad -5.4145, estrictamente dentro, asi que su
multiplicador debe ser cero y el checker lo marca.

### (b) Un registro JM_PS_SINGLETON_COL sobre la misma columna

Escribio -coef*y (linea 1571) y el SINGLETON_ROW lo consume.

lotfi, columna 250: arena[13] (SINGLETON_COL, fila 54, coef -1) se reproduce
primero y deja sol_redcost[250] = +0.001; arena[9] (SINGLETON_ROW, fila 94)
lo consume y publica y_94 = 0.001/1 = 0.001.

finnis, columna 563: arena[178] (fila 261, coef 0.913, y = 72.5128) deja
-66.204; arena[139] (fila 292) lo consume y publica y_292 = -66.204.

### (c) Un registro JM_PS_FIXED_COL

Escribio c_j - suma(a*y) con parte de los duales todavia en cero.

vtp-base, columna 181: arena[248] (FIXED_COL) se reproduce primero, luego
arena[172] (fila 176, tl=0 th=0) se lleva y = 1320.1986408, y arena[171]
(fila 175, th=1) se queda con cero.

### La evidencia para rechazarlo esta grabada y sin usar

El registro lleva row_tightens_lo y row_tightens_hi, escritos en las lineas
739-740 justamente para esta decision. ps_replay_one ya no los lee: las
unicas lecturas restantes son las lineas 1713 y 1716, que pertenecen a
JM_PS_FORCING_ROW donde el campo significa otra cosa (force_hi), y las
1823-1824, que son la rama no optima. El rewrite de 02-04 elimino las
lecturas del camino optimo. En los cuatro casos de (a) la fila que se lleva
el multiplicador tiene tl=0 y th=0.

Aviso por si alguien va directo a esa reparacion: leer los flags es
necesario pero no suficiente. En finnis la fila 292 tiene th=1 y aun asi su
multiplicador esta mal, porque la columna acaba en 0, su bound inferior
original. Dos filas pueden haber apretado y solo la mas ajustada es la
responsable.

### Instancias

Grupo B completo: bnl1, bnl2, e226.
Mitades duales de 25fv47, vtp-base, lotfi, finnis.

## F3 - coste reducido publicado sobre un punto interior (2 de 19)

src/presolve.c:1571:

    orig->sol_redcost[j] = ps_published(-rec->coef * orig->sol_dual[i]);

La cabecera del fichero justifica esto diciendo que una columna de coste 0
no tiene requisito de signo con el que el dual de la fila pueda chocar. Eso
es falso. La familia solo dispara sobre columnas ACOTADAS (!free_col, linea
840), asi que la columna siempre tiene al menos un bound finito, y en cuanto
x_j cae estrictamente entre ellos la factibilidad dual exige d_j = 0.

perold, columna 1325: coste 0, bounds [0, inf), x = 0.0797 interior,
d = +7.8697698. Viene de arena[132], fila 577 (igualdad [-0.1401, -0.1401],
sobrevive, y = -7.8697698).

pilot-we, columna 81: coste 0, [0, inf), x = 1.142669 interior,
d = +6817.1056, que es -(-2.41255)(2825.6906).

El estado que lo dispara: la fila i conserva un dual no nulo en el reduced
solve y el valor recuperado de la singleton column queda interior a
[rec->lo, rec->hi]. El sol_col_status publicado es JAOS_BASIS_BASIC (lineas
1563-1565), que es lo correcto para el valor y lo incompatible con el coste
reducido que se publica dos lineas despues.

pilot87 (dual 0.000739) encaja aqui por forma, pero no lo he verificado
instancia a instancia.

## Por que el objetivo sigue coincidiendo con Koch

JM_PS_SINGLETON_COL solo dispara con m->col_cost[j] == 0.0 (linea 805).
Todas las x mal publicadas tienen coste cero, asi que ni siquiera
recalcular c*x desde el punto corrupto cambiaria el numero. Ademas el
objetivo se publica desde el reduced solve (linea 1833) mas obj_offset,
nunca recalculado desde sol_col. El resultado es un objetivo correcto sobre
un punto infactible.

## Correccion al briefing

PS_NO_ACT no es inerte. Esta en la tabla recuperada:

  finnis:   dual 66.2 -> 0, rsub 0.00246 -> 6.08e-10; las filas se quedan en 3.64e3
  vtp-base: row 1.9e4 -> 26, dual 1320 -> 0, rsub 4.37 -> 2.6e-15

No es el sitio del defecto. Esta en el camino causal de esas dos:
FORCING_ROW fija columnas y empuja registros FIXED_COL, y REDUNDANT_ROW
mata filas y decrementa col_deg, que es otro productor de singleton columns
y de los costes reducidos rancios que consume F2.

## Dos sospechas, sin confirmar

### Varias singleton columns acotadas por la misma fila

La linea 840 comprueba !free_col y nunca row_frozen[i], asi que nada impide
relajar una segunda columna fuera de una fila ya congelada. Cada una
apuntaria a los bounds originales de la fila contra un rest que la anterior
ya empujo hasta el bound. En las 16 instancias medidas hay exactamente un
registro SINGLETON_COL por fila violada (nsingcol=1), asi que es un riesgo
latente, no una causa observada.

Lo zanjaria: una instancia con dos columnas de coste 0 y grado 1 en la
misma fila, o un contador sobre el set estandar de filas con mas de un
registro SINGLETON_COL.

### El camino jm_postsolve_solved

No tiene bucle de siembra de filas supervivientes y hace memset de sol_row
a cero (lineas 1927-1931), asi que rest sale por completo de la acumulacion
LIFO y el defecto F1 se aplica con mas razon. Ninguna de las 19 tomo ese
camino.

Lo zanjaria: un modelo que presolve reduzca a cero columnas conservando una
singleton column acotada.

## Ficheros

Codigo revisado, sin tocar:
  C:/Users/vall-/Desktop/projectes/jaos/src/presolve.c
  C:/Users/vall-/Desktop/projectes/jaos/src/check.c

Drivers y trazas nuevos, en scratch fuera del repo, bajo
C:/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/c244bad1-d761-46da-b840-b90f0be71717/scratchpad/build/

  rows.c              filas violadas con viol - F
  dual2.c             atribucion dual por filas
  col2.c              atribucion dual por columnas
  trace_wrapper.inc   traza de replay bajo JAOS_TRACE_ROW
  trace-ken07.txt     traza completa de la fila 2413
  src/presolve.c.bak  el presolve.c de ese scratch antes del parche de traza

No he tocado ningun fichero del repositorio ni he lanzado campanas.
