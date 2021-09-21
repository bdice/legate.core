/* Copyright 2021 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <numeric>

#include "mapping/mapping.h"

#include "legate_defines.h"

using namespace Legion;

namespace legate {
namespace mapping {

Memory::Kind get_memory_kind(StoreTarget target)
{
  switch (target) {
    case StoreTarget::SYSMEM: return Memory::Kind::SYSTEM_MEM;
    case StoreTarget::FBMEM: return Memory::Kind::GPU_FB_MEM;
    case StoreTarget::ZCMEM: return Memory::Kind::Z_COPY_MEM;
    case StoreTarget::SOCKETMEM: return Memory::Kind::SOCKET_MEM;
    default: LEGATE_ABORT
  }
  assert(false);
  return Memory::Kind::SYSTEM_MEM;
}

bool InstanceMappingPolicy::operator==(const InstanceMappingPolicy& other) const
{
  return target == other.target && allocation == other.allocation && layout == other.layout &&
         exact == other.exact && ordering == other.ordering;
}

bool InstanceMappingPolicy::operator!=(const InstanceMappingPolicy& other) const
{
  return !operator==(other);
}

void InstanceMappingPolicy::populate_layout_constraints(
  Legion::LayoutConstraintSet& layout_constraints) const
{
  std::vector<DimensionKind> dimension_ordering{};
  if (layout == InstLayout::AOS) dimension_ordering.push_back(DIM_F);
  for (auto it = ordering.rbegin(); it != ordering.rend(); ++it)
    dimension_ordering.push_back(static_cast<DimensionKind>(DIM_X + *it));
  if (layout == InstLayout::SOA) dimension_ordering.push_back(DIM_F);
  layout_constraints.add_constraint(OrderingConstraint(dimension_ordering, false /*contiguous*/));

  layout_constraints.add_constraint(MemoryConstraint(get_memory_kind(target)));
}

/*static*/ InstanceMappingPolicy InstanceMappingPolicy::default_policy(StoreTarget target,
                                                                       bool exact)
{
  InstanceMappingPolicy policy{};
  policy.target = target;
  policy.exact  = exact;
  return std::move(policy);
}

bool StoreMapping::for_unbound_stores() const
{
  for (auto& store : stores) return store.unbound();
  assert(false);
  return false;
}

uint32_t StoreMapping::requirement_index() const
{
  assert(stores.size() > 0);
  uint32_t result = -1U;
  for (auto& store : stores) {
    auto idx = store.region_field().index();
    assert(result == -1U || result == idx);
    result = idx;
  }
  return result;
}

void StoreMapping::populate_layout_constraints(
  Legion::LayoutConstraintSet& layout_constraints) const
{
  policy.populate_layout_constraints(layout_constraints);

  std::vector<FieldID> fields{};
  for (auto& store : stores) fields.push_back(store.region_field().field_id());
  layout_constraints.add_constraint(FieldConstraint(fields, true /*contiguous*/));
}

/*static*/ StoreMapping StoreMapping::default_mapping(const Store& store,
                                                      StoreTarget target,
                                                      bool exact)
{
  StoreMapping mapping{};
  mapping.policy = InstanceMappingPolicy::default_policy(target, exact);
  if (!store.unbound()) {
    mapping.policy.ordering.resize(store.dim());
    std::iota(mapping.policy.ordering.begin(), mapping.policy.ordering.end(), 0);
  }
  mapping.stores.push_back(store);
  return std::move(mapping);
}

}  // namespace mapping
}  // namespace legate
