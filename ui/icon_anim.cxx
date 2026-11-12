#include "icon_anim.hxx"

namespace icon_anim {

void   trigger(const void *) noexcept {}
double scale(const void *) noexcept { return 1.0; }
void   prune() noexcept {}

Scope::Scope(cairo_t *cr_, const void *, double, double) noexcept : cr(cr_), active(false) {}
Scope::~Scope() noexcept {}

}
