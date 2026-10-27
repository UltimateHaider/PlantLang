#ifndef PLANT_AD2_H
#define PLANT_AD2_H

/*
 * plant_ad2.h — v0.60.0a: isolated reverse-mode ADv2 (higher-order AD).
 *
 * A SECOND, independent tape engine used by GRAD_HO(...). It chains the
 * existing single forward hook, records into its own tape, and runs a
 * TAPED backward so gradients are themselves differentiable. The original
 * first-order engine (plant_autograd.c / GRAD) is untouched.
 */

#include "plant_tensor.h"

/* Local (ADv2-only) op codes; forward ops reuse OP_* (1..17). */
#define A2_BCAST     100   /* broadcast `in0` to `out`'s shape          */
#define A2_AXIS_SUM  101   /* reduce `in0` to `out`'s shape (adjoint)  */

typedef struct A2Node {
    int op;
    PlantTensor* in0;
    PlantTensor* in1;
    double       scalar;
    PlantTensor* out;
    int          pseudo;   /* `out` was created here (scalar root) */
    struct A2Node* prev;
} A2Node;

typedef struct A2Frame {
    A2Node* head;
    int     frozen;        /* 1 = no longer recording */
    int     level;         /* 0 = forward, 1 = first backward, ... */
    struct A2Frame* prev;
} A2Frame;

/* Idempotent: chains plant_tape_record_hook and starts frame 0. */
void plant_ad2_init(void);

/* Frees every frame (LIFO) and starts a fresh frame 0. */
void plant_ad2_clear(void);

/* GRAD_HO(loss, w): backprop the top frame (taped) → differentiable grad. */
PlantTensor* plant_ad2_grad(double loss_d, PlantTensor* w);

/* TAPE_CLEAR(): clear BOTH engines (first-order + ADv2). */
void plant_tape_clear_both(void);

#endif /* PLANT_AD2_H */
