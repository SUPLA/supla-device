#!/usr/bin/env python3
"""Render MQTT API documentation from operations captured by unit tests."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any


PUBLIC_CATEGORIES = {"public"}
KNOWN_PAYLOAD_TYPES = {"boolean", "integer", "number", "string", "json"}
SECTION_ORDER = {
    "Device": 0,
    "Relay": 1,
    "Roller shutter": 2,
    "Dimmer": 3,
    "RGB controller": 4,
    "Dimmer and RGB controller": 5,
    "Thermometer": 6,
    "Humidity and temperature sensor": 7,
    "HVAC": 8,
    "Electricity meter": 9,
    "Binary sensor": 10,
}


class ValidationError(RuntimeError):
    pass


def load_json(path: Path) -> dict[str, Any]:
    with path.open(encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValidationError(f"{path}: root must be a JSON object")
    return value


def infer_payload_type(payload: str) -> str | None:
    if payload == "":
        return None
    if payload in {"true", "false"}:
        return "boolean"
    if re.fullmatch(r"[-+]?\d+", payload):
        return "integer"
    if re.fullmatch(r"[-+]?(?:\d+\.\d*|\d*\.\d+)", payload):
        return "number"
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return "string"
    return "json" if isinstance(parsed, (dict, list)) else "string"


def collect_operations(observed: dict[str, Any]) -> list[dict[str, Any]]:
    if observed.get("schema_version") != 1:
        raise ValidationError("observed.json: unsupported schema_version")
    scenarios = observed.get("scenarios")
    if not isinstance(scenarios, list):
        raise ValidationError("observed.json: scenarios must be an array")

    operations: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for scenario in scenarios:
        scenario_id = scenario.get("id")
        if not isinstance(scenario_id, str) or not scenario_id:
            raise ValidationError("observed.json: scenario id must be non-empty")
        if scenario_id in seen_ids:
            raise ValidationError(f"duplicate scenario id: {scenario_id}")
        seen_ids.add(scenario_id)
        for operation in scenario.get("operations", []):
            item = dict(operation)
            item["scenario_id"] = scenario_id
            item["category"] = operation.get(
                "category", scenario.get("category", "public")
            )
            item["channel_type"] = scenario.get("channel_type", "")
            item["channel_function"] = scenario.get("channel_function", "")
            operations.append(item)
    return operations


def validate(
    observed: dict[str, Any], metadata: dict[str, Any]
) -> tuple[list[str], list[str], list[dict[str, Any]]]:
    operations = collect_operations(observed)
    metadata_topics = metadata.get("topics", {})
    if not isinstance(metadata_topics, dict):
        raise ValidationError("metadata.json: topics must be an object")

    by_topic: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for operation in operations:
        topic = operation.get("topic")
        if not isinstance(topic, str) or not topic:
            raise ValidationError("observed.json: operation topic must be non-empty")
        by_topic[topic].append(operation)

    errors: list[str] = []
    warnings: list[str] = []
    for topic, topic_operations in sorted(by_topic.items()):
        qos_values = {operation.get("qos") for operation in topic_operations}
        retain_values = {
            operation.get("retain") for operation in topic_operations
        }
        direction_values = {
            operation.get("direction") for operation in topic_operations
        }
        if len(qos_values) > 1:
            errors.append(f"{topic}: observed with different QoS values")
        if len(retain_values) > 1:
            errors.append(f"{topic}: observed with different retain values")
        if len(direction_values) > 1:
            errors.append(f"{topic}: observed in different directions")

        observed_types = {
            inferred
            for operation in topic_operations
            if "payload_example" in operation
            if (inferred := infer_payload_type(operation["payload_example"]))
            is not None
        }
        topic_metadata = metadata_topics.get(topic)
        scenario_payload_types = set()
        if topic_metadata is not None:
            if payload_type := topic_metadata.get("payload_type"):
                scenario_payload_types.add(payload_type)
            overrides = topic_metadata.get("overrides", {})
            if isinstance(overrides, dict):
                for override in overrides.values():
                    if isinstance(override, dict) and (
                        payload_type := override.get("payload_type")
                    ):
                        scenario_payload_types.add(payload_type)

        if (
            len(observed_types) > 1
            and observed_types != {"integer", "number"}
            and not observed_types <= scenario_payload_types
        ):
            errors.append(
                f"{topic}: inconsistent observed payload types: "
                + ", ".join(sorted(observed_types))
            )

        if topic_metadata is None:
            warnings.append(f"observed topic has no metadata: {topic}")
            continue
        payload_type = topic_metadata.get("payload_type")
        if payload_type is not None and payload_type not in KNOWN_PAYLOAD_TYPES:
            errors.append(f"{topic}: unsupported metadata payload_type {payload_type}")
        compatible_types = observed_types
        if payload_type == "number":
            compatible_types = observed_types - {"integer"}
        if (
            payload_type
            and compatible_types
            and compatible_types != {payload_type}
            and not compatible_types <= scenario_payload_types
        ):
            errors.append(
                f"{topic}: metadata payload_type {payload_type} conflicts with "
                f"observed {', '.join(sorted(observed_types))}"
            )

    for topic in sorted(set(metadata_topics) - set(by_topic)):
        warnings.append(f"metadata topic was not observed: {topic}")

    return errors, warnings, operations


def display_payload(operation: dict[str, Any]) -> str:
    if "payload_example" not in operation:
        return "—"
    payload = operation["payload_example"]
    return f"`{payload}`" if payload else "_(empty)_"


def format_json_payload(payload: str) -> str:
    try:
        return json.dumps(json.loads(payload), ensure_ascii=False, indent=2)
    except json.JSONDecodeError:
        return payload


def section_name(channel_type: str, channel_function: str) -> str:
    if channel_type == "device":
        return "Device"
    if channel_type == "SUPLA_CHANNELTYPE_RELAY":
        if channel_function == "SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER":
            return "Roller shutter"
        return "Relay"
    return {
        "SUPLA_CHANNELTYPE_DIMMER": "Dimmer",
        "SUPLA_CHANNELTYPE_RGBLEDCONTROLLER": "RGB controller",
        "SUPLA_CHANNELTYPE_DIMMERANDRGBLED": "Dimmer and RGB controller",
        "SUPLA_CHANNELTYPE_THERMOMETER": "Thermometer",
        "SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR": (
            "Humidity and temperature sensor"
        ),
        "SUPLA_CHANNELTYPE_HVAC": "HVAC",
        "SUPLA_CHANNELTYPE_ELECTRICITY_METER": "Electricity meter",
        "SUPLA_CHANNELTYPE_BINARYSENSOR": "Binary sensor",
    }.get(channel_type, "Channel")


def fallback_title(topic: str) -> str:
    suffix = topic.rstrip("/").rsplit("/", 1)[-1]
    return suffix.replace("_", " ").replace("-", " ").capitalize()


def translated(
    translations: dict[str, Any], group: str, key: str, default: str
) -> str:
    return translations.get(group, {}).get(key, default)


def merge_dicts(base: dict[str, Any], overlay: dict[str, Any]) -> dict[str, Any]:
    result = dict(base)
    for key, value in overlay.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = merge_dicts(result[key], value)
        else:
            result[key] = value
    return result


def operation_variable_key(operation: dict[str, Any]) -> tuple[Any, ...]:
    variables = operation.get("variables", {})
    return tuple(sorted(variables.items()))


def topic_group_name(operation: dict[str, Any]) -> str:
    if operation.get("operation") == "subscribe":
        return "subscribed_topics"
    if operation.get("operation") == "publish":
        return "published_topics"
    return "other_topics"


def section_names(
    operations: list[dict[str, Any]], categories: set[str]
) -> list[str]:
    names = {
        section_name(operation["channel_type"], operation["channel_function"])
        for operation in operations
        if operation["category"] in categories
    }
    return sorted(
        names,
        key=lambda name: (SECTION_ORDER.get(name, len(SECTION_ORDER)), name),
    )


def markdown_anchor(title: str) -> str:
    return re.sub(r"[^\w -]", "", title.lower()).replace(" ", "-")


def render_section_menu(
    operations: list[dict[str, Any]],
    categories: set[str],
    translations: dict[str, Any],
) -> list[str]:
    links = [
        f"[{translated(translations, 'sections', name, name)}]"
        f"(#{markdown_anchor(translated(translations, 'sections', name, name))})"
        for name in section_names(operations, categories)
    ]
    if not links:
        return []
    heading = translated(
        translations,
        "ui",
        "channel_types_and_functions",
        "Channel types and functions",
    )
    return [f"## {heading}", "", " · ".join(links), ""]


def resolve_topic_metadata(
    metadata_topics: dict[str, Any], topic: str, scenario_ids: list[str]
) -> dict[str, Any]:
    info = dict(metadata_topics.get(topic, {}))
    overrides = info.pop("overrides", {})
    for scenario_id in scenario_ids:
        override = overrides.get(scenario_id)
        if isinstance(override, dict):
            info.update(override)
    return info


def render_topic_sections(
    operations: list[dict[str, Any]],
    metadata: dict[str, Any],
    categories: set[str],
    translations: dict[str, Any],
) -> list[str]:
    metadata_topics = metadata.get("topics", {})
    groups: dict[str, dict[str, Any]] = {}
    for operation in operations:
        if operation["category"] not in categories:
            continue
        name = section_name(
            operation["channel_type"], operation["channel_function"]
        )
        group = groups.setdefault(
            name,
            {"technical": set(), "topics": defaultdict(list)},
        )
        group["technical"].add(
            (operation["channel_type"], operation["channel_function"])
        )
        group["topics"][operation["topic"]].append(operation)

    lines: list[str] = []
    sorted_groups = sorted(
        groups.items(),
        key=lambda item: (
            SECTION_ORDER.get(item[0], len(SECTION_ORDER)),
            item[0],
        ),
    )
    for name, group in sorted_groups:
        display_name = translated(translations, "sections", name, name)
        topics = group["topics"]
        technical = sorted(group["technical"])
        channel_types = sorted(
            {channel_type for channel_type, _ in technical if channel_type != "device"}
        )
        channel_functions = sorted(
            {
                channel_function
                for channel_type, channel_function in technical
                if channel_type != "device" and channel_function
            }
        )
        lines.extend([f"## {display_name}", ""])
        if channel_types:
            key = "channel_type" if len(channel_types) == 1 else "channel_types"
            default = "Channel type" if len(channel_types) == 1 else "Channel types"
            label = translated(translations, "ui", key, default)
            values = ", ".join(f"`{value}`" for value in channel_types)
            lines.extend([f"{label}: {values}", ""])
        if channel_functions:
            key = (
                "channel_function"
                if len(channel_functions) == 1
                else "channel_functions"
            )
            default = (
                "Channel function" if len(channel_functions) == 1 else "Channel functions"
            )
            label = translated(translations, "ui", key, default)
            values = ", ".join(f"`{value}`" for value in channel_functions)
            lines.extend([f"{label}: {values}", ""])
        topics_by_operation: dict[str, list[tuple[str, list[dict[str, Any]]]]] = (
            defaultdict(list)
        )
        for topic, topic_operations in sorted(topics.items()):
            topics_by_operation[topic_group_name(topic_operations[0])].append(
                (topic, topic_operations)
            )
        topic_group_order = {
            "subscribed_topics": 0,
            "published_topics": 1,
            "other_topics": 2,
        }
        for topic_group, grouped_topics in sorted(
            topics_by_operation.items(),
            key=lambda item: (topic_group_order.get(item[0], 3), item[0]),
        ):
            topic_group_label = translated(
                translations,
                "ui",
                topic_group,
                topic_group.replace("_", " ").capitalize(),
            )
            lines.extend([f"### {topic_group_label}", ""])
            for topic, topic_operations in grouped_topics:
                topic_operations = sorted(
                    topic_operations,
                    key=lambda operation: (
                        operation_variable_key(operation),
                        operation.get("payload_example", ""),
                    ),
                )
                first = topic_operations[0]
                scenario_ids = sorted(
                    {operation["scenario_id"] for operation in topic_operations}
                )
                info = resolve_topic_metadata(
                    metadata_topics, topic, scenario_ids
                )
                title = info.get("title", fallback_title(topic))
                is_json_payload = info.get("payload_type") == "json"
                if "example" in info:
                    examples = [f"`{info['example']}`"]
                else:
                    examples = []
                    for operation in topic_operations:
                        if "payload_example" not in operation:
                            continue
                        payload = operation["payload_example"]
                        example = (
                            format_json_payload(payload)
                            if is_json_payload
                            else display_payload(operation)
                        )
                        if example not in examples:
                            examples.append(example)
                    if not examples and info.get("tested_payloads"):
                        examples = [f"`{info['tested_payloads'][0]}`"]
                    if not examples and info.get("allowed_values"):
                        examples = [f"`{info['allowed_values'][0]}`"]
                lines.extend(
                    [
                        f"#### {title}",
                        "",
                        f"- {translated(translations, 'ui', 'topic', 'Topic')}: `{topic}`",
                        f"- {translated(translations, 'ui', 'payload_type', 'Payload type')}: "
                        f"`{info.get('payload_type', 'undocumented')}`",
                    ]
                )
                if "allowed_values" in info:
                    values = ", ".join(
                        f"`{value}`" for value in info["allowed_values"]
                    )
                    label = translated(
                        translations, "ui", "allowed_values", "Allowed values"
                    )
                    lines.append(f"- {label}: {values}")
                if "range" in info:
                    label = translated(translations, "ui", "range", "Range")
                    lines.append(f"- {label}: `{info['range']}`")
                if "unit" in info:
                    label = translated(translations, "ui", "unit", "Unit")
                    lines.append(f"- {label}: `{info['unit']}`")
                lines.extend(
                    [
                        f"- QoS: `{first['qos']}`",
                        f"- Retain: `{str(first['retain']).lower()}`",
                        f"- {translated(translations, 'ui', 'availability', 'Availability')}: "
                        f"{info.get('availability', translated(translations, 'ui', 'not_documented', 'Not documented.'))}",
                        "",
                        info.get("description", "No manual description yet."),
                        "",
                    ]
                )
                if "compatibility" in info:
                    lines.extend([info["compatibility"], ""])
                if examples:
                    key = "example" if len(examples) == 1 else "examples"
                    default = "Example" if len(examples) == 1 else "Examples"
                    label = translated(translations, "ui", key, default)
                    if is_json_payload:
                        single_label = translated(
                            translations, "ui", "example", "Example"
                        )
                        for index, example in enumerate(examples, start=1):
                            example_label = (
                                label
                                if len(examples) == 1
                                else f"{single_label} {index}"
                            )
                            lines.extend(
                                [f"{example_label}:", "", "```json", example, "```", ""]
                            )
                    else:
                        lines.extend([f"{label}: {', '.join(examples)}", ""])
    return lines


def render(
    observed: dict[str, Any],
    metadata: dict[str, Any],
    translations: dict[str, Any] | None = None,
) -> tuple[str, str, list[str]]:
    translations = translations or {}
    errors, warnings, operations = validate(observed, metadata)
    if errors:
        raise ValidationError("\n".join(errors))

    localized_metadata = merge_dicts(metadata, translations.get("metadata", {}))
    main_lines = [
        f"# {translated(translations, 'ui', 'document_title', 'MQTT topics')}",
        "",
        translated(
            translations,
            "ui",
            "language_switcher",
            "Languages: **English** · [Polski](topics.pl.md)",
        ),
        "",
        *translations.get(
            "intro",
            [
                "This document describes the MQTT topics exposed by SUPLA devices ",
                "operating in MQTT mode.",
                "",
                "`{prefix}` means `[custom-prefix/]supla/devices/{hostname}`. ",
                "`{channel}` is the channel number. ",
                "`{phase}` is the electricity-meter phase number.",
                "",
                "Topics under **Subscribed topics** accept commands sent to the device. ",
                "Topics under **Published topics** contain states and measurements ",
                "published by the device.",
                "",
                "The exact set of available topics depends on the device configuration, ",
                "channel types, channel functions, and supported measurements.",
                "",
            ],
        ),
    ]
    main_lines.extend(
        render_section_menu(operations, PUBLIC_CATEGORIES, translations)
    )
    main_lines.extend(
        render_topic_sections(
            operations, localized_metadata, PUBLIC_CATEGORIES, translations
        )
    )

    has_home_assistant_operations = any(
        operation["category"] == "home_assistant" for operation in operations
    )
    ha_lines = [
        f"# {translated(translations, 'ui', 'ha_title', 'Home Assistant Discovery')}",
        "",
    ]
    if has_home_assistant_operations:
        ha_lines.extend(
            [
                translated(
                    translations,
                    "ui",
                    "ha_separate",
                    "Discovery traffic is generated separately from the public MQTT API.",
                ),
                "",
            ]
        )
    else:
        ha_lines.extend(
            [
                translated(
                    translations,
                    "ui",
                    "ha_empty",
                    "Home Assistant Discovery capture infrastructure is available, but no discovery scenarios are included yet.",
                ),
                "",
            ]
        )
    ha_lines.extend(
        render_topic_sections(
            operations, localized_metadata, {"home_assistant"}, translations
        )
    )
    if not has_home_assistant_operations:
        ha_lines.extend(
            [
                translated(
                    translations,
                    "ui",
                    "ha_no_topics",
                    "No discovery topics are documented in this version.",
                ),
                "",
            ]
        )

    return "\n".join(main_lines), "\n".join(ha_lines), warnings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--observed", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--home-assistant-output", type=Path, required=True)
    parser.add_argument("--translations", type=Path)
    args = parser.parse_args()

    try:
        translations = load_json(args.translations) if args.translations else {}
        main_document, ha_document, warnings = render(
            load_json(args.observed), load_json(args.metadata), translations
        )
    except (OSError, json.JSONDecodeError, ValidationError) as error:
        print(f"mqtt-docgen: error: {error}", file=sys.stderr)
        return 1

    for warning in warnings:
        print(f"mqtt-docgen: warning: {warning}", file=sys.stderr)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.home_assistant_output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(main_document, encoding="utf-8")
    args.home_assistant_output.write_text(ha_document, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
