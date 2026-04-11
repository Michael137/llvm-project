//===-- TestOptionValue.cpp --------        -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/OptionValues.h"
#include "lldb/Interpreter/Property.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace lldb_private;

class Callback {
public:
  virtual void Invoke() const {}
  void operator()() const { Invoke(); }

protected:
  ~Callback() = default;
};

class MockCallback final : public Callback {
public:
  MOCK_CONST_METHOD0(Invoke, void());
};

// Test a single-value class.
TEST(OptionValueString, DeepCopy) {
  OptionValueString str;
  str.SetValueFromString("ab");

  MockCallback callback;
  str.SetValueChangedCallback([&callback] { callback(); });
  EXPECT_CALL(callback, Invoke());

  auto copy_sp = str.DeepCopy(nullptr);

  // Test that the base class data members are copied/set correctly.
  ASSERT_TRUE(copy_sp);
  ASSERT_EQ(copy_sp->GetParent().get(), nullptr);
  ASSERT_TRUE(copy_sp->OptionWasSet());
  ASSERT_EQ(copy_sp->GetValueAs<llvm::StringRef>(), "ab");

  // Trigger the callback.
  copy_sp->SetValueFromString("c", eVarSetOperationAppend);
  ASSERT_EQ(copy_sp->GetValueAs<llvm::StringRef>(), "abc");
}

// Test an aggregate class.
TEST(OptionValueArgs, DeepCopy) {
  OptionValueArgs args;
  args.SetValueFromString("A B");

  MockCallback callback;
  args.SetValueChangedCallback([&callback] { callback(); });
  EXPECT_CALL(callback, Invoke());

  auto copy_sp = args.DeepCopy(nullptr);

  // Test that the base class data members are copied/set correctly.
  ASSERT_TRUE(copy_sp);
  ASSERT_EQ(copy_sp->GetParent(), nullptr);
  ASSERT_TRUE(copy_sp->OptionWasSet());

  auto *args_copy_ptr = copy_sp->GetAsArgs();
  ASSERT_EQ(args_copy_ptr->GetSize(), 2U);
  ASSERT_EQ((*args_copy_ptr)[0]->GetParent(), copy_sp);
  ASSERT_EQ((*args_copy_ptr)[0]->GetValueAs<llvm::StringRef>(), "A");
  ASSERT_EQ((*args_copy_ptr)[1]->GetParent(), copy_sp);
  ASSERT_EQ((*args_copy_ptr)[1]->GetValueAs<llvm::StringRef>(), "B");

  // Trigger the callback.
  copy_sp->SetValueFromString("C", eVarSetOperationAppend);
  ASSERT_TRUE(args_copy_ptr);
  ASSERT_EQ(args_copy_ptr->GetSize(), 3U);
  ASSERT_EQ((*args_copy_ptr)[2]->GetValueAs<llvm::StringRef>(), "C");
}

class TestProperties : public OptionValueProperties {
public:
  static std::shared_ptr<TestProperties> CreateGlobal() {
    auto props_sp = std::make_shared<TestProperties>();
    const bool is_global = false;

    auto dict_sp = std::make_shared<OptionValueDictionary>(1 << eTypeUInt64);
    props_sp->AppendProperty("dict", "", is_global, dict_sp);

    auto file_list_sp = std::make_shared<OptionValueFileSpecList>();
    props_sp->AppendProperty("file-list", "", is_global, file_list_sp);
    return props_sp;
  }

  void SetDictionaryChangedCallback(const MockCallback &callback) {
    SetValueChangedCallback(m_dict_index, [&callback] { callback(); });
  }

  void SetFileListChangedCallback(const MockCallback &callback) {
    SetValueChangedCallback(m_file_list_index, [&callback] { callback(); });
  }

  OptionValueDictionary *GetDictionary() {
    return GetPropertyAtIndexAsOptionValueDictionary(m_dict_index);
  }

  OptionValueFileSpecList *GetFileList() {
    return GetPropertyAtIndexAsOptionValueFileSpecList(m_file_list_index);
  }

private:
  lldb::OptionValueSP Clone() const override {
    return std::make_shared<TestProperties>(*this);
  }

  uint32_t m_dict_index = 0;
  uint32_t m_file_list_index = 1;
};

// Test a user-defined propery class.
TEST(TestProperties, DeepCopy) {
  auto props_sp = TestProperties::CreateGlobal();
  props_sp->GetDictionary()->SetValueFromString("A=1 B=2");
  props_sp->GetFileList()->SetValueFromString("path/to/file");

  MockCallback callback;
  props_sp->SetDictionaryChangedCallback(callback);
  props_sp->SetFileListChangedCallback(callback);
  EXPECT_CALL(callback, Invoke()).Times(2);

  auto copy_sp = props_sp->DeepCopy(nullptr);

  // Test that the base class data members are copied/set correctly.
  ASSERT_TRUE(copy_sp);
  ASSERT_EQ(copy_sp->GetParent(), nullptr);

  // This cast is safe only if the class overrides Clone().
  auto *props_copy_ptr = static_cast<TestProperties *>(copy_sp.get());
  ASSERT_TRUE(props_copy_ptr);

  // Test the first child.
  auto dict_copy_ptr = props_copy_ptr->GetDictionary();
  ASSERT_TRUE(dict_copy_ptr);
  ASSERT_EQ(dict_copy_ptr->GetParent(), copy_sp);
  ASSERT_TRUE(dict_copy_ptr->OptionWasSet());
  ASSERT_EQ(dict_copy_ptr->GetNumValues(), 2U);

  auto value_ptr = dict_copy_ptr->GetValueForKey("A");
  ASSERT_TRUE(value_ptr);
  ASSERT_EQ(value_ptr->GetParent().get(), dict_copy_ptr);
  ASSERT_EQ(value_ptr->GetValueAs<uint64_t>(), 1U);

  value_ptr = dict_copy_ptr->GetValueForKey("B");
  ASSERT_TRUE(value_ptr);
  ASSERT_EQ(value_ptr->GetParent().get(), dict_copy_ptr);
  ASSERT_EQ(value_ptr->GetValueAs<uint64_t>(), 2U);

  // Test the second child.
  auto file_list_copy_ptr = props_copy_ptr->GetFileList();
  ASSERT_TRUE(file_list_copy_ptr);
  ASSERT_EQ(file_list_copy_ptr->GetParent(), copy_sp);
  ASSERT_TRUE(file_list_copy_ptr->OptionWasSet());

  auto file_list_copy = file_list_copy_ptr->GetCurrentValue();
  ASSERT_EQ(file_list_copy.GetSize(), 1U);
  ASSERT_EQ(file_list_copy.GetFileSpecAtIndex(0), FileSpec("path/to/file"));

  // Trigger the callback first time.
  dict_copy_ptr->SetValueFromString("C=3", eVarSetOperationAppend);

  // Trigger the callback second time.
  file_list_copy_ptr->SetValueFromString("0 another/path",
                                         eVarSetOperationReplace);
}

// Test that a Property with eTypeDictionary and a default_cstr_value
// initializes the dictionary with the provided key=value entries.
TEST(OptionValueDictionary, DefaultValuesFromPropertyDefinition) {
  // Set up enum values for the dictionary elements.
  static constexpr OptionEnumValueElement g_test_enum_values[] = {
      {0, "off", "Disabled"},
      {1, "on", "Enabled"},
      {2, "warn", "Warn only"},
  };

  // Create a PropertyDefinition with a default dictionary value.
  PropertyDefinition def{
      "test-dict",
      OptionValue::eTypeDictionary,
      /*global=*/false,
      /*default_uint_value=*/OptionValue::eTypeEnum,
      /*default_cstr_value=*/"foo=on bar=warn",
      /*enum_values=*/OptionEnumValues(g_test_enum_values),
      /*description=*/"test dictionary",
  };

  Property prop(def);
  auto *dict = prop.GetValue()->GetAsDictionary();
  ASSERT_TRUE(dict);
  ASSERT_EQ(dict->GetNumValues(), 2u);

  auto foo = dict->GetValueForKey("foo");
  ASSERT_TRUE(foo);
  ASSERT_TRUE(foo->GetAsEnumeration());
  EXPECT_EQ(foo->GetAsEnumeration()->GetCurrentValue(), 1);

  auto bar = dict->GetValueForKey("bar");
  ASSERT_TRUE(bar);
  ASSERT_TRUE(bar->GetAsEnumeration());
  EXPECT_EQ(bar->GetAsEnumeration()->GetCurrentValue(), 2);
}
