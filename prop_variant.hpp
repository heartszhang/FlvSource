#pragma once

struct _prop_variant_t : PROPVARIANT{
  _prop_variant_t();
  ~_prop_variant_t();
  _prop_variant_t(const _prop_variant_t&);
  _prop_variant_t(PROPVARIANT const*);
  _prop_variant_t&operator=(_prop_variant_t&);
  _prop_variant_t&operator=(PROPVARIANT const*);
};