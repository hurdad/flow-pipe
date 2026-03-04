#include "flowpipe/payload.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace flowpipe {
namespace {

TEST(PayloadMetaTest, AttrsAreLazyAndClearable) {
  PayloadMeta meta;
  EXPECT_FALSE(meta.has_attrs());
  EXPECT_EQ(meta.get_attr("missing"), nullptr);

  meta.set_attr("pipeline.tenant_id", std::string("acme"));
  ASSERT_TRUE(meta.has_attrs());

  const auto* tenant = meta.get_attr("pipeline.tenant_id");
  ASSERT_NE(tenant, nullptr);
  ASSERT_TRUE(std::holds_alternative<std::string>(*tenant));
  EXPECT_EQ(std::get<std::string>(*tenant), "acme");

  meta.clear_attrs();
  EXPECT_FALSE(meta.has_attrs());
  EXPECT_EQ(meta.get_attr("pipeline.tenant_id"), nullptr);
}

TEST(PayloadMetaTest, CopyOnWriteDetachesOnMutation) {
  PayloadMeta base;
  base.set_attr("pipeline.partition", int64_t{3});

  PayloadMeta copy = base;
  ASSERT_TRUE(base.attrs);
  ASSERT_TRUE(copy.attrs);
  EXPECT_EQ(base.attrs.get(), copy.attrs.get());

  copy.set_attr("pipeline.partition", int64_t{8});

  ASSERT_TRUE(base.attrs);
  ASSERT_TRUE(copy.attrs);
  EXPECT_NE(base.attrs.get(), copy.attrs.get());

  const auto* base_value = base.get_attr("pipeline.partition");
  ASSERT_NE(base_value, nullptr);
  EXPECT_EQ(std::get<int64_t>(*base_value), 3);

  const auto* copy_value = copy.get_attr("pipeline.partition");
  ASSERT_NE(copy_value, nullptr);
  EXPECT_EQ(std::get<int64_t>(*copy_value), 8);
}

TEST(PayloadMetaTest, EraseAttrUsesDetachedMapAndResetsWhenEmpty) {
  PayloadMeta first;
  first.set_attr("pipeline.a", int64_t{1});
  first.set_attr("pipeline.b", int64_t{2});

  PayloadMeta second = first;
  ASSERT_TRUE(first.attrs);
  ASSERT_TRUE(second.attrs);
  EXPECT_EQ(first.attrs.get(), second.attrs.get());

  EXPECT_TRUE(second.erase_attr("pipeline.a"));
  ASSERT_TRUE(first.get_attr("pipeline.a") != nullptr);
  EXPECT_EQ(second.get_attr("pipeline.a"), nullptr);

  EXPECT_TRUE(second.erase_attr("pipeline.b"));
  EXPECT_FALSE(second.has_attrs());
  EXPECT_FALSE(second.erase_attr("pipeline.missing"));
}

}  // namespace
}  // namespace flowpipe
