#!/usr/bin/env python3

import importlib.util
import json
import unittest
from pathlib import Path


SPEC = importlib.util.spec_from_file_location(
    "mqtt_doc_render", Path(__file__).with_name("render.py")
)
render = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(render)


def observed(*operations):
    return {
        "schema_version": 1,
        "scenarios": [
            {
                "id": "scenario",
                "category": "public",
                "channel_type": "TYPE",
                "channel_function": "FUNCTION",
                "operations": list(operations),
            }
        ],
    }


def operation(qos=0, retain=True, payload="true"):
    return {
        "direction": "device_to_broker",
        "operation": "publish",
        "topic": "{prefix}/topic",
        "payload_example": payload,
        "qos": qos,
        "retain": retain,
    }


class RenderTests(unittest.TestCase):
    def test_polish_translation_covers_all_public_metadata_text(self):
        repo_root = Path(__file__).resolve().parents[3]
        metadata = json.loads(
            (repo_root / "extras/docs/mqtt/metadata.json").read_text(
                encoding="utf-8"
            )
        )["topics"]
        translations = json.loads(
            (repo_root / "extras/docs/mqtt/translations.pl.json").read_text(
                encoding="utf-8"
            )
        )["metadata"]["topics"]
        self.assertEqual(set(metadata), set(translations))

        text_fields = {"title", "description", "availability", "compatibility"}
        for topic, info in metadata.items():
            translated_info = translations[topic]
            for field in text_fields & set(info):
                self.assertIn(field, translated_info, f"{topic}: {field}")
            for scenario_id, override in info.get("overrides", {}).items():
                translated_override = translated_info.get("overrides", {}).get(
                    scenario_id, {}
                )
                for field in text_fields & set(override):
                    self.assertIn(
                        field,
                        translated_override,
                        f"{topic}/{scenario_id}: {field}",
                    )

    def test_public_intro_describes_api_without_generator_internals(self):
        main, _, _ = render.render(observed(operation()), {"topics": {}})
        self.assertIn(
            "This document describes the MQTT topics exposed by SUPLA devices",
            main,
        )
        self.assertIn(
            "Topics under **Subscribed topics** accept commands", main
        )
        self.assertNotIn("unit-test scenarios", main)
        self.assertNotIn("inventory of every possible runtime topic", main)

    def test_polish_translation_localizes_ui_sections_and_metadata(self):
        value = observed(operation())
        value["scenarios"][0]["channel_type"] = "SUPLA_CHANNELTYPE_RELAY"
        translations = {
            "ui": {
                "document_title": "Topiki MQTT",
                "language_switcher": "Języki: [English](topics.md) · **Polski**",
                "channel_types_and_functions": "Typy i funkcje kanałów",
                "published_topics": "Publikowane topiki",
                "topic": "Topik",
                "payload_type": "Typ payloadu",
                "availability": "Dostępność",
                "example": "Przykład",
            },
            "sections": {"Relay": "Przekaźnik"},
            "metadata": {
                "topics": {
                    "{prefix}/topic": {
                        "title": "Stan przekaźnika",
                        "description": "Bieżący stan.",
                        "availability": "Kanały przekaźnikowe.",
                    }
                }
            },
        }
        main, _, _ = render.render(value, {"topics": {}}, translations)
        self.assertIn("# Topiki MQTT", main)
        self.assertIn("[Przekaźnik](#przekaźnik)", main)
        self.assertIn("## Przekaźnik", main)
        self.assertIn("### Publikowane topiki", main)
        self.assertIn("#### Stan przekaźnika", main)
        self.assertIn("- Topik: `{prefix}/topic`", main)
        self.assertIn("- Dostępność: Kanały przekaźnikowe.", main)
        self.assertIn("Przykład: `true`", main)

    def test_missing_metadata_is_warning(self):
        errors, warnings, _ = render.validate(observed(operation()), {"topics": {}})
        self.assertEqual([], errors)
        self.assertEqual(
            ["observed topic has no metadata: {prefix}/topic"], warnings
        )

    def test_qos_conflict_is_error(self):
        errors, _, _ = render.validate(
            observed(operation(qos=0), operation(qos=1)), {"topics": {}}
        )
        self.assertIn(
            "{prefix}/topic: observed with different QoS values", errors
        )

    def test_retain_conflict_is_error(self):
        errors, _, _ = render.validate(
            observed(operation(retain=True), operation(retain=False)),
            {"topics": {}},
        )
        self.assertIn(
            "{prefix}/topic: observed with different retain values", errors
        )

    def test_direction_conflict_is_error(self):
        subscribe = operation()
        subscribe["direction"] = "broker_to_device"
        subscribe["operation"] = "subscribe"
        subscribe.pop("payload_example")
        errors, _, _ = render.validate(
            observed(operation(), subscribe), {"topics": {}}
        )
        self.assertIn(
            "{prefix}/topic: observed in different directions", errors
        )

    def test_metadata_payload_conflict_is_error(self):
        errors, _, _ = render.validate(
            observed(operation(payload="true")),
            {"topics": {"{prefix}/topic": {"payload_type": "integer"}}},
        )
        self.assertIn("conflicts with observed boolean", errors[0])

    def test_inconsistent_observed_payload_types_are_error(self):
        errors, _, _ = render.validate(
            observed(operation(payload="true"), operation(payload="text")),
            {"topics": {}},
        )
        self.assertIn("inconsistent observed payload types", errors[0])

    def test_unobserved_metadata_is_warning(self):
        _, warnings, _ = render.validate(
            observed(operation()),
            {"topics": {"{prefix}/stale": {"payload_type": "string"}}},
        )
        self.assertIn(
            "metadata topic was not observed: {prefix}/stale", warnings
        )

    def test_cleanup_operation_is_not_in_public_document(self):
        cleanup = operation(payload="")
        cleanup["category"] = "cleanup"
        main, _, _ = render.render(observed(cleanup), {"topics": {}})
        self.assertNotIn("### {prefix}/topic", main)

    def test_friendly_section_name_keeps_cpp_metadata(self):
        value = observed(operation())
        value["scenarios"][0]["channel_type"] = "SUPLA_CHANNELTYPE_RELAY"
        value["scenarios"][0]["channel_function"] = (
            "SUPLA_CHANNELFNC_POWERSWITCH"
        )
        main, _, _ = render.render(value, {"topics": {}})
        self.assertIn("## Relay", main)
        self.assertIn("Channel type: `SUPLA_CHANNELTYPE_RELAY`", main)
        self.assertNotIn("## SUPLA_CHANNELTYPE_RELAY", main)

    def test_topics_are_grouped_by_subscription_and_publication(self):
        subscribe = operation()
        subscribe["direction"] = "broker_to_device"
        subscribe["operation"] = "subscribe"
        subscribe["topic"] = "{prefix}/channels/{channel}/set/on"
        subscribe.pop("payload_example")
        publish = operation()
        publish["topic"] = "{prefix}/channels/{channel}/state/on"
        value = observed(subscribe, publish)
        value["scenarios"][0]["channel_type"] = "SUPLA_CHANNELTYPE_RELAY"
        main, _, _ = render.render(value, {"topics": {}})
        self.assertIn("### Subscribed topics", main)
        self.assertIn("### Published topics", main)
        self.assertLess(
            main.index("`{prefix}/channels/{channel}/set/on`"),
            main.index("### Published topics"),
        )
        self.assertNotIn("- Direction:", main)
        self.assertNotIn("- Operation:", main)

    def test_channel_type_and_function_menu_links_to_sections(self):
        value = observed(operation())
        value["scenarios"][0]["channel_type"] = "SUPLA_CHANNELTYPE_RELAY"
        main, _, _ = render.render(value, {"topics": {}})
        self.assertIn("## Channel types and functions", main)
        self.assertIn("[Relay](#relay)", main)
        self.assertLess(main.index("[Relay](#relay)"), main.index("## Relay"))

    def test_availability_is_rendered_but_optional_flag_is_not(self):
        metadata = {
            "topics": {
                "{prefix}/topic": {
                    "availability": "Only while the reading is valid.",
                    "optional": True,
                }
            }
        }
        main, _, _ = render.render(observed(operation()), metadata)
        self.assertIn("Availability: Only while the reading is valid.", main)
        self.assertNotIn("Optional:", main)

    def test_scenario_override_provides_contextual_topic_metadata(self):
        metadata = {
            "topics": {
                "{prefix}/topic": {
                    "title": "Generic action",
                    "availability": "All channels.",
                    "overrides": {
                        "scenario": {
                            "title": "Relay action",
                            "availability": "Relay channels.",
                            "allowed_values": ["turn_on", "turn_off"],
                        }
                    },
                }
            }
        }
        main, _, _ = render.render(observed(operation()), metadata)
        self.assertIn("#### Relay action", main)
        self.assertIn("Availability: Relay channels.", main)
        self.assertIn("- Allowed values: `turn_on`, `turn_off`", main)
        self.assertNotIn("#### Generic action", main)

    def test_example_label_does_not_expose_its_source(self):
        main, _, _ = render.render(
            observed(operation(payload="0.0.0.0")),
            {"topics": {"{prefix}/topic": {"example": "192.0.2.10"}}},
        )
        self.assertIn("Example: `192.0.2.10`", main)
        self.assertNotIn("Observed payload example", main)

        main, _, _ = render.render(observed(operation(payload="true")), {"topics": {}})
        self.assertIn("Example: `true`", main)

    def test_roller_shutter_has_distinct_friendly_section(self):
        value = observed(operation())
        value["scenarios"][0]["channel_type"] = "SUPLA_CHANNELTYPE_RELAY"
        value["scenarios"][0]["channel_function"] = (
            "SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER"
        )
        main, _, _ = render.render(value, {"topics": {}})
        self.assertIn("## Roller shutter", main)

    def test_fallback_title_uses_topic_suffix(self):
        subscribe = operation()
        subscribe["topic"] = "{prefix}/channels/{channel}/execute_action"
        main, _, _ = render.render(observed(subscribe), {"topics": {}})
        self.assertIn("### Execute action", main)
        self.assertNotIn(
            "### {prefix}/channels/{channel}/execute_action", main
        )

    def test_payload_evidence_is_kept_out_of_public_document(self):
        metadata = {
            "topics": {
                "{prefix}/topic": {
                    "tested_payloads": ["true", "false"],
                    "confirmed_by_tests": ["MqttTests.processData"],
                }
            }
        }
        main, _, _ = render.render(observed(operation()), metadata)
        self.assertIn("Example: `true`", main)
        self.assertNotIn("Tested payload", main)
        self.assertNotIn("Payload confirmed by tests", main)
        self.assertNotIn("Confirmed by:", main)

    def test_placeholder_variables_order_payload_examples(self):
        phase_two = operation(payload="20")
        phase_two["variables"] = {"phase": 2}
        phase_one = operation(payload="10")
        phase_one["variables"] = {"phase": 1}
        main, _, _ = render.render(
            observed(phase_two, phase_one), {"topics": {}}
        )
        self.assertIn("Examples: `10`, `20`", main)

    def test_json_payload_is_rendered_as_a_formatted_code_block(self):
        value = observed(operation(payload='{"name":"Thermostat","qos":0}'))
        main, _, _ = render.render(
            value,
            {"topics": {"{prefix}/topic": {"payload_type": "json"}}},
        )
        self.assertIn("Example:", main)
        self.assertIn("```json\n{\n  \"name\": \"Thermostat\",", main)

    def test_mixed_scenario_keeps_public_operation_only(self):
        public = operation()
        public["topic"] = "{prefix}/public"
        public["category"] = "public"
        cleanup = operation(payload="")
        cleanup["topic"] = "{prefix}/obsolete"
        cleanup["category"] = "cleanup"
        value = observed(public, cleanup)
        value["scenarios"][0]["category"] = "mixed"
        main, _, _ = render.render(value, {"topics": {}})
        self.assertIn("`{prefix}/public`", main)
        self.assertNotIn("`{prefix}/obsolete`", main)

    def test_same_friendly_section_aggregates_channel_functions(self):
        value = observed(operation())
        first = value["scenarios"][0]
        first["id"] = "power"
        first["channel_type"] = "SUPLA_CHANNELTYPE_RELAY"
        first["channel_function"] = "SUPLA_CHANNELFNC_POWERSWITCH"
        second = {
            **first,
            "id": "light",
            "channel_function": "SUPLA_CHANNELFNC_LIGHTSWITCH",
            "operations": [operation()],
        }
        value["scenarios"].append(second)
        main, _, _ = render.render(value, {"topics": {}})
        self.assertEqual(1, main.count("## Relay"))
        self.assertIn("Channel functions:", main)

    def test_home_assistant_is_not_in_main_document(self):
        value = observed(operation())
        value["scenarios"][0]["category"] = "home_assistant"
        main, home_assistant, _ = render.render(value, {"topics": {}})
        self.assertNotIn("### {prefix}/topic", main)
        self.assertIn("#### Topic", home_assistant)

    def test_home_assistant_empty_document_explains_capture_status(self):
        _, home_assistant, _ = render.render(observed(operation()), {"topics": {}})
        self.assertIn(
            "capture infrastructure is available, but no discovery scenarios",
            home_assistant,
        )


if __name__ == "__main__":
    unittest.main()
