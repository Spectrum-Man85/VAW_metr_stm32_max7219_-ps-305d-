#ifndef FILTERS_H
#define FILTERS_H

void init_filters();
void get_filters();
void filter_get_vv();
void filter_get_aa();

void step_update_display_from_filtered(float filtered_v, float filtered_a);

#endif
