// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_

#include <vector>

#include "net/base/net_export.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_partition_key.h"

namespace net {

// A data structure used to represent a collection of cookie partition keys.
//
// It can represent all possible cookie partition keys when
// `contains_all_keys_` is true.
//
// It can also represent a finite number of cookie partition keys, including
// zero.
class NET_EXPORT CookiePartitionKeyCollection {
 public:
  // Creates an empty key collection.
  explicit CookiePartitionKeyCollection();
  CookiePartitionKeyCollection(const CookiePartitionKeyCollection& other);
  CookiePartitionKeyCollection(CookiePartitionKeyCollection&& other);
  // Creates a key collection with a single element.
  explicit CookiePartitionKeyCollection(const CookiePartitionKey& key);
  // Creates a set that contains each partition key in the vector.
  explicit CookiePartitionKeyCollection(
      const std::vector<CookiePartitionKey>& keys);

  CookiePartitionKeyCollection& operator=(
      const CookiePartitionKeyCollection& other);
  CookiePartitionKeyCollection& operator=(CookiePartitionKeyCollection&& other);
  ~CookiePartitionKeyCollection();

  static CookiePartitionKeyCollection ContainsAll() {
    return CookiePartitionKeyCollection(true);
  }

  static CookiePartitionKeyCollection FromOptional(
      const absl::optional<CookiePartitionKey>& opt_key) {
    return opt_key ? CookiePartitionKeyCollection(opt_key.value())
                   : CookiePartitionKeyCollection();
  }

  // Takes a CookiePartitionKeyCollection which was created in a context that
  // does not have access to sites' First-Party Set owners and converts it to
  // the correct First-Party-Sets-aware CookiePartitionKeyCollection, replacing
  // any CookiePartitionKeys whose sites which are members of a set with a new
  // partition key containing the set's owner site.
  CookiePartitionKeyCollection FirstPartySetify(
      const CookieAccessDelegate* cookie_access_delegate) const;

  // Temporary method used to record where we need to decide how to build the
  // CookiePartitionKeyCollection.
  //
  // Returns an empty key collection, so no partitioned cookies will be returned
  // at callsites this is used.
  //
  // TODO(crbug.com/1225444): Remove this method and update callsites to use
  // appropriate constructor.
  static CookiePartitionKeyCollection Todo() {
    return CookiePartitionKeyCollection();
  }

  // CookieMonster can check if the key collection is empty to avoid searching
  // the PartitionedCookieMap at all.
  bool IsEmpty() const { return !contains_all_keys_ && keys_.empty(); }

  // Returns if the key collection contains every partition key.
  bool ContainsAllKeys() const { return contains_all_keys_; }

  // Iterate over all keys in the key collection, do not call this method if
  // `contains_all_keys` is true.
  const std::vector<CookiePartitionKey>& PartitionKeys() const {
    DCHECK(!contains_all_keys_);
    return keys_;
  }

 private:
  explicit CookiePartitionKeyCollection(bool contains_all_keys_);

  bool contains_all_keys_ = false;
  // If `contains_all_keys_` is true, `keys_` must be empty.
  // If `keys_` is not empty, then `contains_all_keys_` must be false.
  std::vector<CookiePartitionKey> keys_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_COLLECTION_H_
