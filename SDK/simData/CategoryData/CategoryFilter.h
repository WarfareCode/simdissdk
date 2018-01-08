/* -*- mode: c++ -*- */
/****************************************************************************
 *****                                                                  *****
 *****                   Classification: UNCLASSIFIED                   *****
 *****                    Classified By:                                *****
 *****                    Declassify On:                                *****
 *****                                                                  *****
 ****************************************************************************
 *
 *
 * Developed by: Naval Research Laboratory, Tactical Electronic Warfare Div.
 *               EW Modeling & Simulation, Code 5773
 *               4555 Overlook Ave.
 *               Washington, D.C. 20375-5339
 *
 * License for source code at https://simdis.nrl.navy.mil/License.aspx
 *
 * The U.S. Government retains all rights to use, duplicate, distribute,
 * disclose, or release this software.
 *
 */
#ifndef SIMDATA_CATEGORYFILTER_H
#define SIMDATA_CATEGORYFILTER_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include "simCore/Common/Common.h"
#include "simData/CategoryData/CategoryNameManager.h"

namespace simData {

/**
* Class for providing a regular expression filter to apply to category data. Implement this class and provide a factory to
* the CategoryFilter to support use of regular expression filtering on category data
*/
class RegExpFilter
{
public:
  /** Destructor */
  virtual ~RegExpFilter() {}

  /**
  * Returns true if the test string matches anything in the regular expression
  * @param[in] test string to test
  * @return true if test string matches
  */
  virtual bool match(const std::string& test) const = 0;

  /**
  * Returns the regex pattern string
  * @return the regex pattern
  */
  virtual std::string pattern() const = 0;
};

typedef std::shared_ptr<RegExpFilter> RegExpFilterPtr;

/** Factory class for creating RegExpFilter objects */
class RegExpFilterFactory
{
public:

  virtual ~RegExpFilterFactory() {}
  /** create a new RegExpFilter object based on the specified expression. */
  virtual RegExpFilterPtr createRegExpFilter(const std::string& expression) = 0;
};

typedef std::shared_ptr<RegExpFilterFactory> RegExpFilterFactoryPtr;

class DataStore;

/**
 * Class to manage the category data filtering.  The CategoryFilter builds an internal map
 * of all categories and their values, with a check state for each.  If the check state is set
 * to true, the filter assumes this category value is required.  If the check state is set to false,
 * the filter assumes this value should be rejected.  Setting a simData::CategoryNameManager::NO_CATEGORY_VALUE_AT_TIME
 * for a category indicates whether to accept or reject if an entity does not have the specified category.
 * Setting a simData::CategoryNameManager::UNLISTED_CATEGORY_VALUE for a category indicates whether to accept or reject
 * if an entity has no corresponding value for this category, although some value does exist (i.e. the category value
 * in the entity is not in the category filter at all.
 *
 * The simData::CategoryFilter class supports serialization and deserialization to string.  The serialization matches
 * the following set of rules:
 *
 *  1. Categories are separated by the backtick (`) character.  Values inside categories are separated by the tilde
 *     (~) character.
 *     - Example: "Color(1)~Blue(0)~Green(0)`Shape(1)~Round(1)~Square(0)" tests the categories Color and
 *       Shape.  Values under Color that are tested for match are Blue and Green.  Values under Shape that
 *       are tested are Round and Square.
 *
 *  2. Categories not listed in the filter will not impact filter results.
 *     - Example: "Color(1)~Blue(1)" will not compare the category Shape.
 *     - Example: "Color(1)~Blue(1)`Shape(1)~Round(1)" will compare the Color and Shape categories, but not the
 *       (missing and not specified) Size category.
 *
 *  3. Empty string (i.e. empty filter) matches all entities.  This is a "specialization" of the more general rule #2.
 *     - Example: " " matches all entities regardless of category settings.
 *
 *  4. Category names that are unchecked (0) do not contribute to the filter, regardless of the check state for values
 *     under that category.  CategoryFilter::serialize() will omit the entire category.
 *     - Example: "Color(0)~Blue(0)" matches all entities regardless of category settings.  It is equivalent to " ".
 *     - Example: "Color(0)~Blue(1)" also matches all entities, and is also equivalent to " ".
 *     - Example: "Color(0)~Unlisted Value(0)" also matches all entities, and is also equivalent to " ".
 *     - Example: "Color(0)~Blue(0)`Shape(1)~Round(1)" will only match entities that are Shape=Round.  It will not
 *       compare the category Color.  This is equivalent to "Shape(1)~Round(1)".
 *
 *  5. Unlisted values are unchecked by default.  The special value "Unlisted Value" can be used to change this behavior.
 *     - Example: "Color(1)~Blue(1)" will match entity with Color=Blue, but will not match entity with Color=Red.
 *     - Example: "Color(1)~Unlisted Value(0)~Blue(0)" will match no entities and is a useless filter.
 *     - Example: "Color(1)~Unlisted Value(0)~Green(1)" will match only entities with Color=Green.  It will not match
 *       entities with Color=Blue, Color=Gray, or entities without a Color.  The simplification is "Color(1)~Green(1)".
 *     - Example: "Color(1)~Unlisted Value(1)~Blue(0)" will not match entity with Color=Blue, but will match entity
 *       with Color=Red.
 *     - Example: "Color(1)~Unlisted Value(1)~Blue(1)" will match all entities and can be simplified to " ".
 *     - Example: "Color(1)~Unlisted Value(1)~Green(1)~Blue(0)" will match Color=Green and Color=Gray, but will not
 *       match Color=Blue.  This is equivalent to "Color(1)~Unlisted Value(1)~Blue(0)".
 *
 *  6. The reserved term "No Value" will match when a category does not have a value for an entity at a given time.
 *     It is NOT included as an unlisted value when using the "Unlisted Value" keyword.
 *     - Example: "Color(1)~Unlisted Value(1)~No Value(0)" will match only entities with a valid Color category value
 *       at the current time.  The Color value could be set to anything, as long as it is set to something.  This
 *       is equivalent to "Color(1)~Unlisted Value(1)" because No Value is not included in the Unlisted Values.
 *     - Example: "Color(1)~Unlisted Value(1)" will match only entities with a valid Color category value at the
 *       current time.  This is equivalent to the previous example.
 *     - Example: "Color(1)~No Value(1)" will match entities that have no value for Color, but will not match an
 *       entity with any valid value in the Color category.
 *
 *  7. All listed categories must match for a filter to pass (match).  Categories are compared with Boolean AND.
 *     - Example: "Color(1)~Green(1)`Shape(1)~Round(1)" will only match entities that have Color=Green AND Shape=Round.
 *       Blue Round entities will fail the filter.  Green Square entities will also fail the filter.  Green entities
 *       without a Shape will also fail the filter.
 */
class SDKDATA_EXPORT CategoryFilter
{
public:
  typedef std::map<int, int> CurrentCategoryValues;    ///< holds current name/value pairs
  typedef std::map<int, bool> ValuesCheck;             ///< holds value int to checked state
  typedef std::pair<bool, ValuesCheck> CategoryValues; ///< holds name checked state and values
  typedef std::map<int, CategoryValues> CategoryCheck; ///< holds name int to name checked state and values
  typedef std::map<int, RegExpFilterPtr> CategoryRegExp; ///< holds name int to list of regexp

  /** Constructor
   * @param dataStore The datastore for the category data
   * @param autoUpdate  If true the Category Filter automatically updates and there is no need to call buildPrefRulesCategoryFilter()
   *    which can be very slow.  If false the Category Filter maintains its original behavior which requires the owner
   *    to call buildPrefRulesCategoryFilter for every change.
   */
  explicit CategoryFilter(simData::DataStore* dataStore, bool autoUpdate = false);

  /** Copy Constructor */
  CategoryFilter(const CategoryFilter& other);

  /** Destructor */
  virtual ~CategoryFilter();

  /** Assignment operator */
  CategoryFilter& operator=(const CategoryFilter& other);

  /**
  * build the category filter based on what is in the data store. Will add default No Value and Unlisted entries for all categories
  */
  void buildPrefRulesCategoryFilter();

  /**
  * Get the current category values for an entity from the data store in the CurrentCategoryValues format
  * @param[in] dataStore Data store to operate on
  * @param[in] entityId  entity for whom to retrieve the current category values
  * @param[out] curVals  map to fill out with category name/value int pairs
  */
  static void getCurrentCategoryValues(simData::DataStore& dataStore, uint64_t entityId, CurrentCategoryValues& curVals);

  /**
  * Get a reference to the current CategoryCheck structure, which is (re)built internally by the call to buildCategoryFilter
  * @return CategoryCheck&  reference to the CategoryCheck structure
  */
  const CategoryCheck& getCategoryFilter() const;

  /**
  * Get reference to this CategoryFilter's data store
  * @return DataStore*
  */
  simData::DataStore* getDataStore() const;

  /**
  * Update every category name and value check stat
  * @param[in] value  new value to set for all check states
  */
  void updateAll(bool value);

  /**
  * Update the check state of a category name
  * @param[in] nameInt  int value of a category name
  * @param[in] nameChecked  value to set for all the category value check states under this name
  */
  void updateCategoryFilterName(int nameInt, bool nameChecked);

  /**
  * Update the check state of a category value
  * @param[in] nameInt  int value of the category name
  * @param[in] valueInt  int value of the category value
  * @param[in] valueChecked  check state for this category value
  */
  void updateCategoryFilterValue(int nameInt, int valueInt, bool valueChecked);

  /**
  * Update the regExp for the specified category name. This regular expression will be used to match against the value
  * for this category name. Pass in empty string to remove the entry for the specified category name.
  * @param[in] nameInt  int value of the category name
  * @param[in] regExp  regular expression filter to apply to the category value
  */
  void updateCategoryFilterRegExp(int nameInt, const std::string& regExp);

  /**
  * Check if the category data of the specified entity matches the current category filter. This is a convenience method
  * that queries the DataStore to get the current category data values of the specified entity, which can be expensive
  * @param[in] entityId  entity to which filter should be tested
  * @return true if this entity's category data passes the filter, false otherwise
  */
  bool match(uint64_t entityId) const;

  /**
  * Check if the category data values passed in match the current category filter.
  * @param[in] curCategoryData  the current category data values for this entity in the DataStore
  * @return true if the specified category data passes the filter, false otherwise
  */
  bool matchData(const CurrentCategoryValues& curCategoryData) const;

  /**
  * Serialize the category filter into a SIMDIS 9 compatible string
  * @param simplify if true, return " " if all category values are checked
  * @return string  the serialized category filter
  */
  std::string serialize(bool simplify = true) const;

  /**
  * De-serialize a category filter string from a SIMDIS 9 compatible string
  * @param checksString serialization of the category filter
  * @param skipEmptyCategories if true, optimize filter by skipping unchecked categories
  * @return true on success, false if there is any problem
  */
  bool deserialize(const std::string &checksString, bool skipEmptyCategories = true);

  /** Simplifies the category filter, removing names and values that do not contribute to filtering. */
  void simplify();

  /** Clears out the filter, removing all checks and resetting to equivalent of " " */
  void clear();

  /**
   * Set the check state of a category value, creating the category and value if necessary.
   * @param[in] nameInt  int value of the category name
   * @param[in] valueInt  int value of the category value
   * @param[in] valueChecked  check state for this category value
   */
  void setValue(int nameInt, int valueInt, bool valueChecked);
  /** Removes the entire category name and all values under. */
  void removeName(int nameInt);
  /** Removes the value entirely from the filter.  If the name is empty, it is also removed.  Returns 0 on success. */
  int removeValue(int nameInt, int valueInt);

  /** Retrieves a list of names that are included in this filter. */
  void getNames(std::vector<int>& names) const;
  /** Retrieves the values associated with the name. */
  void getValues(int nameInt, ValuesCheck& checks) const;

  /** Set the factory for creating RegExpFilter objects. */
  void setRegExpFilterFactory(RegExpFilterFactoryPtr factory);

private:
  class CategoryFilterListener;

  /**
  * build the category filter based on what is in the data store.
  * @param[in] addNoValue  whether to add a default no value entry for each category
  * @param[in] noValue  default no value check state
  * @param[in] addUnlisted  whether to add an unlisted entry for each category
  * @param[in] unlisted  default unlisted check state
  */
  void buildCategoryFilter_(bool addNoValue, bool noValue, bool addUnlisted, bool unlisted);
  /** Returns true if all RegExpFilters match. Returns false if anything fails to match */
  bool matchRegExpFilter_(const CurrentCategoryValues& curCategoryData) const;
  /** Reduces the categoryCheck_ to the smallest state possible. */
  void simplify_(CategoryCheck& checks) const;
  /** Remove all entries with the same value as Unlisted Value. */
  void simplifyValues_(CategoryFilter::CategoryCheck& checks) const;
  /** If all values are on, including the top level item, drop the item from the map. */
  void simplifyCategories_(CategoryFilter::CategoryCheck& checks) const;
  /** Add the passed in category name plus the values of "Unlisted Value" and "No Value" */
  void addCategoryName_(int nameIndex);
  /** Add the passed in value for the given name */
  void addCategoryValue_(int nameIndex, int valueIndex);
  /** Clear all data */
  void clear_();

  simData::DataStore* dataStore_; ///< reference to the data store
  RegExpFilterFactoryPtr regExpFactory_; ///< factory for creating RegExpFilter objects
  bool autoUpdate_; ///< If true the Category Filter automatically updates and there is no need to call buildPrefRulesCategoryFilter
  CategoryCheck categoryCheck_; ///< category filter structure
  CategoryRegExp categoryRegExp_; ///< category reg exp filter structure
  simData::CategoryNameManager::ListenerPtr listenerPtr_;
};

}

#endif /* SIMDATA_CATEGORYFILTER_H */
