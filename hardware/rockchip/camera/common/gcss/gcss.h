/*
 * Copyright (C) 2016-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __GCSS_H__
#define __GCSS_H__

#include <ia_tools/css_types.h>
#include <ia_cipf/ia_cipf_types.h>

#ifdef __cplusplus

#include <string>
#include <map>
#include <utility>
#include <vector>

namespace GCSS {

/* For now there is no C API to GCSS so exposing the ItemUID and IGraphConfig
 * here */
class ItemUID
{
public:
    static ia_uid str2key(const std::string&);
    static const char* key2str(const ia_uid key);
    static ia_uid generateKey(const std::string&);
    static void addCustomKeyMap(std::map<std::string, ia_uid> &osMap);

    ItemUID(std::initializer_list<ia_uid> uids) {
        mUids.insert(mUids.end(), uids.begin(), uids.end());
    };
    ItemUID() {};
    void pop_back() { mUids.pop_back(); };
    void push_back( const ia_uid& iuid) { mUids.push_back(iuid); };

    bool operator == (const ItemUID& v) const {
        return v.mUids == mUids;
    }
    std::string toString() const;

    std::size_t size() const { return mUids.size(); };

    ItemUID(const ItemUID &ref) { mUids = ref.mUids; };

    ia_uid& operator[](std::size_t idx) { return mUids[idx]; }
    const ia_uid& operator[](std::size_t idx) const
    { return mUids[idx]; }

private:
    friend bool operator<(const ItemUID &r, const ItemUID &l);
    friend bool operator>(const ItemUID &r, const ItemUID &l);
    std::vector<ia_uid> mUids;

};

class IGraphConfig
{
public:
    virtual ~IGraphConfig() {}

    /**
     * Get root level node of the graph
     *
     * \ingroup gcss
     *
     * \return pointer to root node on success
     * \return nullptr if error
     */
    virtual IGraphConfig* getRoot(void) const = 0;
    /*
     * Get ancestor for the node
     *
     * \ingroup gcss
     *
     * \return pointer to ancestor node on success
     * \return nullptr if error
     */
    virtual IGraphConfig* getAncestor(void) const = 0;

    /**
     * Iterate descendants by index
     *
     * \ingroup gcss
     *
     *  Iterates descendants from the given index and returns pointer to the
     *  first descendant that matches given gcss key. Index is
     *  being moved to match the returned descendant. Returns nullptr if no
     *  descendant is found.
     * \param[in] the gcss key of the attribute by which we want to search.
     *            ie. GCSS_KEY_TYPE, when iterating descendants by type.
     * \param[in/out] index position, advanced to index of found descendant
     * \return pointer to IGraphConfig at the iterator location
     */
    virtual IGraphConfig* iterateDescendantByIndex(const ia_uid,
                                                   uint32_t &index) const = 0;
    /**
     *  Iterates descendants starting from the given index and returns pointer
     *  to the first descendant where attribute and its value match the ones given
     *  as parameters. Index is being moved to match the returned descendant.
     *  Returns nullptr if no descendant is found.
     * \param[in] the gcss key of the attribute by which we want to search.
     *            ie. GCSS_KEY_TYPE, when iterating descendants by type.
     * \param[in] value to search for. For string type keys ie. GCSS_KEY_TYPE,
     *            the value is given as GCSS_KEY. When searching for type kernel,
     *            the value should be GCSS_KEY_KERNEL. For int type keys ie.
     *            GCSS_KEY_ID, the value can be int.
     * \param[in/out] index position, advanced to index of found descendant
     * \return pointer to IGraphConfig at the iterator location
     * \return nullptr if error
     */
    virtual IGraphConfig* iterateDescendantByIndex(const ia_uid key,
                                                   const ia_uid value,
                                                   uint32_t &index) const = 0;
    virtual uint32_t getDescendantCount() const = 0;

    /**
     * Get descendant by GCSS_KEY
     *
     * \ingroup gcss
     *
     * \return pointer to the descendant if success
     * \return nullptr if error
     */
    virtual IGraphConfig* getDescendant(ia_uid) const = 0;

    /**
     * Get descendant by GCSS_KEY, or array of keys
     *
     * \ingroup gcss
     *
     * possible usages:
     * getDescendant(GCSS_KEY_GRAPH);
     * getDescendant({GCSS_KEY_GRAPH, GCSS_KEY_CONNECTION});
     *
     * \return pointer to the descendant if success
     * \return nullptr if error
     */

    virtual IGraphConfig* getDescendant(const ItemUID&) const = 0;
    /**
     * DEPRECATED
     * Get descendant by string
     *
     * \ingroup gcss
     *
     * \return pointer to the descendant if success
     * \return nullptr if error
     */
    virtual IGraphConfig* getDescendantByString(const std::string &str) const = 0;

    /**
     * Get int value for an attribute given as GCSS_KEY
     *
     * \ingroup gcss
     *
     * \param[in] gcss key
     * \param[out] int value
     * \return css_err_none if success
     */
    virtual css_err_t getValue(ia_uid, int&) const = 0;
    virtual css_err_t getValue(const ItemUID&, int&) const = 0;

    /**
     * Get str value for an attribute given as GCSS_KEY
     *
     * \ingroup gcss
     *
     * \param[in] gcss key
     * \param[out] str value
     * \return css_err_none if success
     */
    virtual css_err_t getValue(ia_uid, std::string&) const = 0;
    virtual css_err_t getValue(const ItemUID&, std::string&) const = 0;

    /**
     * Set int value for an attribute given as GCSS_KEY
     *
     * \ingroup gcss
     *
     * \param[in] gcss key
     * \param[in] int value
     * \return css_err_none if success
     */

    virtual css_err_t setValue(ia_uid, int) = 0;
    virtual css_err_t setValue(const ItemUID&, int) = 0;

    /**
     * Set str value for an attribute given as GCSS_KEY
     *
     * \ingroup gcss
     *
     * \param[in] gcss key
     * \param[in] str value
     * \return css_err_none if success
     */
    virtual css_err_t setValue(ia_uid, const std::string&) = 0;
    virtual css_err_t setValue(const ItemUID&, const std::string&) = 0;
};
} // namespace

extern "C" {
#endif

#define GCSS_KEY(key, str) GCSS_KEY_##key,
#define GCSS_KEY_SECTION_START(key, str, val) GCSS_KEY_##key = val,
typedef enum _GraphConfigKey {
    #include "gcss_keys.h"
    GCSS_KEY_START_CUSTOM_KEYS = 0x8000,
} GraphConfigKey;
#undef GCSS_KEY
#undef GCSS_KEY_SECTION_START

#ifdef __cplusplus
}
#endif

#endif
