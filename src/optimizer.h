#ifndef OPTIMIZER_H
#define OPTIMIZER_H
#pragma once

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

void optimize(struct Ast *ast);

#ifdef __cplusplus
}
#endif

#endif
