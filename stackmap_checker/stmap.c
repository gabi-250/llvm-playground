#include <stdlib.h>
#include <string.h>
#include "stmap.h"

stack_map_t* create_stack_map(uint8_t *start_addr) {
   stack_map_t *sm = (stack_map_t *)malloc(sizeof(stack_map_t));
   char *addr = (char *)start_addr;
   size_t header_size = sizeof(uint8_t) * 2 + sizeof(uint16_t) + 3 * sizeof(uint32_t);
   memcpy(sm, (void *)addr, header_size);
   sm->version = *(uint8_t *)addr;
   addr += header_size;
   sm->stack_size_records =
      (stack_size_record_t *)malloc(sizeof(stack_size_record_t) * sm->num_func);
   for (size_t i = 0; i < sm->num_func; ++i) {
      stack_size_record_t *rec = sm->stack_size_records + i;
      memcpy(rec, (void *)addr, sizeof(stack_size_record_t));
      addr += sizeof(stack_size_record_t);
   }
   sm->constants = (uint64_t *)malloc(sizeof(uint64_t) * sm->num_const);
   for (size_t i = 0; i < sm->num_const; ++i) {
      uint64_t *constant = sm->constants + i;
      *constant = *(uint64_t *)addr;
      addr += sizeof(uint64_t);
   }
   sm->sm_records =
      (stack_map_record_t *)malloc(sizeof(stack_map_record_t) * sm->num_rec);
   size_t record_size = sizeof(uint64_t) + sizeof(uint32_t) + 2 * sizeof(uint16_t);
   for (size_t i = 0; i < sm->num_rec; ++i) {
      stack_map_record_t *rec = sm->sm_records + i;
      // copy the first 4 fields
      memcpy(rec, addr, record_size);
      addr += record_size;
      rec->locations =
         (location_t *)malloc(sizeof(location_t) * rec->num_locations);
      size_t loc_size =
         2 * sizeof(uint8_t) + 3 * sizeof(uint16_t) + sizeof(uint32_t);
      for (size_t j = 0; j < rec->num_locations; ++j) {
         location_t *loc = rec->locations + j;
         memcpy(loc, addr, loc_size);
         addr += loc_size;
      }
      if ((rec->num_locations * sizeof(location_t)) % 8 != 0) {
         addr += 4; // padding
      }
      addr += 2; // padding
      rec->num_liveouts = *(uint16_t *)addr;
      addr += 2;
      rec->liveouts = (liveout_t *)malloc(sizeof(liveout_t) * rec->num_liveouts);
      size_t liveout_size = sizeof(uint16_t) + 2 * sizeof(uint8_t);
      for (int j = 0; j < rec->num_liveouts; ++j) {
         liveout_t *liveout = rec->liveouts + j;
         memcpy(liveout, addr, liveout_size);
         addr += liveout_size;
      }
      if ((rec->num_liveouts * sizeof(liveout_t) + 4) % 8 != 0) {
         addr += 4; // padding
      }
   }
   return sm;
}

void free_stack_map(stack_map_t *sm) {
   free(sm->stack_size_records);
   free(sm->constants);
   for (size_t i = 0; i < sm->num_rec; ++i) {
      stack_map_record_t *rec = sm->sm_records + i;
      free(rec->locations);
      free(rec->liveouts);
   }
   free(sm->sm_records);
   free(sm);
}