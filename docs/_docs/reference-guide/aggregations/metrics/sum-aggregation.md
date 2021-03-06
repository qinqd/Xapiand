---
title: Sum Aggregation
---

A _single-value_ metrics aggregation that sums up numeric values that are
extracted from the aggregated documents.

## Structuring

The following snippet captures the structure of sum aggregations:

```json
"<aggregation_name>": {
  "_sum": {
      "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, we can sum the balances of all accounts in the state of Indiana with:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": {
    "contact.state": "Indiana"
  },
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "indiana_total_balance": {
      "_sum": {
        "_field": "balance"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Resulting in:

```json
{
    "#aggregations": {
        "_doc_count": 17,
        "indiana_total_balance": {
            "_sum": 2565033.04
        }
    },
    ...
}
```
