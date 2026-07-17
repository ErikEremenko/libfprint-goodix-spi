// SPDX-License-Identifier: LGPL-2.1-or-later
/* Copyright (C) 2026 Berke Kabagöz <berkekbgz@gmail.com> */

/* Native exploratory mirror of AlgoChicago+0x5ea80 under-capacity study. */

#include <stdio.h>

#define reflect_101 feature_reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-feature.c"
#undef reflect_101
#include "../drivers/gdix51c0/gdix51c0-chicago-enrollment.c"
#define affine_from_three_points match_affine_from_three_points
#include "../drivers/gdix51c0/gdix51c0-chicago-match.c"
#undef affine_from_three_points

static guint capacity_fallback_call_index;

static void
analyzer_capacity_relation_builder (
  const Gdix51c0ChicagoSubtemplateView *gallery,
  const Gdix51c0ChicagoSubtemplateView *probe,
  Gdix51c0ChicagoRelation              *relation)
{
  const gchar *dump_index =
    g_getenv ("CHICAGO_DUMP_CAPACITY_FALLBACK_INDEX");

  if (dump_index &&
      (guint) g_ascii_strtoull (dump_index, NULL, 0) ==
        capacity_fallback_call_index)
    {
      Gdix51c0ChicagoMatchPair pairs[GDIX51C0_CHICAGO_MATCH_PAIR_LIMIT];
      Gdix51c0ChicagoMatchPair forward[GDIX51C0_CHICAGO_MATCH_PAIR_LIMIT];
      Gdix51c0ChicagoMatchPair capacity[GDIX51C0_CHICAGO_MATCH_PAIR_LIMIT];
      const guint count = gdix51c0_chicago_match_ordinary_correspondences (
        probe->records, probe->record_count, probe->active_count,
        gallery->records, gallery->record_count, gallery->active_count,
        pairs);
      const guint forward_count =
        gdix51c0_chicago_match_ordinary_correspondences (
          gallery->records, gallery->record_count, gallery->active_count,
          probe->records, probe->record_count, probe->active_count, forward);
      const guint capacity_count = ordinary_correspondences_with_gates (
        gallery->records, gallery->record_count, gallery->active_count,
        probe->records, probe->record_count, probe->active_count,
        22, 45, capacity);

      printf ("native-capacity-fallback-pair-vector index=%u",
              capacity_fallback_call_index);
      for (guint index = 0; index < count; index++)
        printf (" %d:%d", pairs[index].old_index, pairs[index].new_index);
      printf ("\n");
      printf ("native-capacity-fallback-forward-vector index=%u",
              capacity_fallback_call_index);
      for (guint index = 0; index < forward_count; index++)
        printf (" %d:%d", forward[index].old_index,
                forward[index].new_index);
      printf ("\n");
      printf ("native-capacity-fallback-config-vector index=%u",
              capacity_fallback_call_index);
      for (guint index = 0; index < capacity_count; index++)
        printf (" %d:%d", capacity[index].old_index,
                capacity[index].new_index);
      printf ("\n");
    }
  gdix51c0_chicago_match_build_capacity_relation (gallery, probe, relation);
  capacity_fallback_call_index++;
}

int
main (int argc, char **argv)
{
  g_autofree gchar *gallery_data = NULL;
  g_autofree gchar *probe_data = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(Gdix51c0ChicagoEnrollment) gallery = NULL;
  g_autoptr(Gdix51c0ChicagoEnrollment) probe = NULL;
  g_autoptr(GBytes) packed = NULL;
  g_autofree Gdix51c0ChicagoRelation *relations = NULL;
  Gdix51c0ChicagoSubtemplateView probe_view;
  Gdix51c0ChicagoMatchScoreRecord score_record;
  Gdix51c0ChicagoMatchGeometry score_geometry;
  Gdix51c0ChicagoMatchTemplateResult match_result;
  Gdix51c0ChicagoSubtemplateView selected_view;
  gsize gallery_size;
  gsize probe_size;
  gconstpointer packed_data;
  gsize packed_size;
  guint selected;
  guint probe_index = 0;
  gint32 capacity_probe_score;
  gint32 capacity_scores[GDIX51C0_CHICAGO_ENROLLMENT_CAPACITY];
  gint32 capacity_qualities[GDIX51C0_CHICAGO_ENROLLMENT_CAPACITY];
  guint capacity_unresolved = 0;
  guint capacity_replacement = G_MAXUINT;

  if (argc != 5)
    {
      fprintf (stderr, "usage: %s gallery.bin probe.bin selected output.bin\n",
               argv[0]);
      return 2;
    }
  selected = (guint) g_ascii_strtoull (argv[3], NULL, 0);
  if (g_getenv ("CHICAGO_STUDY_PROBE_INDEX"))
    probe_index = (guint) g_ascii_strtoull (
      g_getenv ("CHICAGO_STUDY_PROBE_INDEX"), NULL, 0);
  if (!g_file_get_contents (argv[1], &gallery_data, &gallery_size, &error) ||
      !g_file_get_contents (argv[2], &probe_data, &probe_size, &error))
    goto fail;
  gallery = gdix51c0_chicago_enrollment_unpack (
    (const guint8 *) gallery_data, gallery_size, &error);
  probe = gdix51c0_chicago_enrollment_unpack (
    (const guint8 *) probe_data, probe_size, &error);
  if (!gallery || !probe ||
      !gdix51c0_chicago_enrollment_get_subtemplate (probe, probe_index,
                                                     &probe_view))
    goto fail;
  if (g_getenv ("CHICAGO_GALLERY_FIRST_INDEX"))
    {
      guint requested = (guint) g_ascii_strtoull (
        g_getenv ("CHICAGO_GALLERY_FIRST_INDEX"), NULL, 0);
      guint schedule_index;

      if (requested >= gallery->subtemplates->len)
        return 3;
      for (schedule_index = 0;
           schedule_index < gallery->subtemplates->len;
           schedule_index++)
        if (gallery->match_order[schedule_index] == requested)
          break;
      if (schedule_index >= gallery->subtemplates->len)
        return 3;
      gallery->match_order[schedule_index] = gallery->match_order[0];
      gallery->match_order[0] = requested;
    }
  if (selected >= gallery->subtemplates->len)
    return 3;
  if (!gdix51c0_chicago_enrollment_get_subtemplate (
        gallery, selected, &selected_view))
    return 4;
  gdix51c0_chicago_match_score_subtemplate_type24 (
    &selected_view, &probe_view, &score_record, &score_geometry);
  printf ("native-study-score selected=%u record=", selected);
  for (guint value = 0; value < sizeof (score_record) / sizeof (gint32); value++)
    printf ("%s%d", value ? "," : "", ((gint32 *) &score_record)[value]);
  printf (" geometry=%d,%d,%d,%d,%d,%d/%u\n",
          score_geometry.transform[0], score_geometry.transform[1],
          score_geometry.transform[2], score_geometry.transform[3],
          score_geometry.transform[4], score_geometry.transform[5],
          score_geometry.inlier_count);
  if (g_getenv ("CHICAGO_DUMP_ALL_RELATIONS"))
    for (guint index = 0; index < gallery->subtemplates->len; index++)
      {
        Gdix51c0ChicagoSubtemplateView gallery_view;

        if (gdix51c0_chicago_enrollment_get_subtemplate (
              gallery, index, &gallery_view))
          {
            gdix51c0_chicago_match_score_subtemplate_type24 (
              &gallery_view, &probe_view, &score_record, &score_geometry);
            printf ("native-study-relation[%u]=%u/%d,%d,%d,%d,%d,%d score=%d\n",
                    index, score_geometry.inlier_count,
                    score_geometry.transform[0], score_geometry.transform[1],
                    score_geometry.transform[2], score_geometry.transform[3],
                    score_geometry.transform[4], score_geometry.transform[5],
                    score_record.secondary_geometry_count);
          }
      }
  gdix51c0_chicago_match_template_type24 (
    gallery, &probe_view, 207, &match_result);
  if (!match_result.selected_index_known ||
      match_result.selected_index != (gint32) selected ||
      match_result.study_relation_count != gallery->subtemplates->len)
    {
      fprintf (stderr, "native study match did not select %u\n", selected);
      return 5;
    }
  relations = g_memdup2 (match_result.study_relations,
                          match_result.study_relation_count *
                            sizeof (*relations));
  if (g_getenv ("CHICAGO_CAPACITY_RELATIONS_IN"))
    {
      g_autofree gchar *relation_data = NULL;
      gsize relation_size;

      if (!g_file_get_contents (g_getenv ("CHICAGO_CAPACITY_RELATIONS_IN"),
                                &relation_data, &relation_size, &error) ||
          relation_size != gallery->subtemplates->len * sizeof (*relations))
        goto fail;
      memcpy (relations, relation_data, relation_size);
    }
  if (g_getenv ("CHICAGO_CAPACITY_GALLERY_RELATIONS_IN"))
    {
      g_autofree gchar *relation_data = NULL;
      gsize relation_size;

      if (!g_file_get_contents (
            g_getenv ("CHICAGO_CAPACITY_GALLERY_RELATIONS_IN"),
            &relation_data, &relation_size, &error) ||
          relation_size != gallery->relations->len * sizeof (*relations))
        goto fail;
      memcpy (gallery->relations->data, relation_data, relation_size);
    }
  if (g_getenv ("CHICAGO_CAPACITY_FORCE_UNRESOLVED_INDEX"))
    {
      const guint target = (guint) g_ascii_strtoull (
        g_getenv ("CHICAGO_CAPACITY_FORCE_UNRESOLVED_INDEX"), NULL, 0);
      const Gdix51c0ChicagoRelation selected_relation = relations[selected];

      if (target >= gallery->subtemplates->len ||
          !gdix51c0_chicago_enrollment_synthesize_capacity_relations (
            gallery, NULL, gallery->subtemplates->len))
        return 6;
      for (guint other = 0; other < gallery->subtemplates->len; other++)
        {
          Gdix51c0ChicagoRelation *relation;

          relations[other].inlier_count = 0;
          memcpy (relations[other].transform,
                  (const gint32[6]) { 0x100, 0, 0, 0, 0x100, 0 },
                  sizeof (relations[other].transform));
          if (other == target)
            continue;
          relation = (Gdix51c0ChicagoRelation *)
            relation_between_subtemplates (
              gallery, MAX (target, other), MIN (target, other));
          if (relation)
            {
              const guint relation_index =
                (guint) (relation -
                         (Gdix51c0ChicagoRelation *) gallery->relations->data);

              relation->inlier_count = 0;
              if (target == selected)
                for (gint edge = (gint) gallery->group_edges->len - 1;
                     edge >= 0; edge--)
                  if (g_array_index (gallery->group_edges, guint, edge) ==
                      relation_index)
                    g_array_remove_index (gallery->group_edges,
                                          (guint) edge);
            }
        }
      if (selected != target)
        relations[selected] = selected_relation;
      printf ("native-capacity-forced-unresolved=%u\n", target);
    }
  if (g_getenv ("CHICAGO_CAPACITY_SELECT") &&
      !(g_getenv ("CHICAGO_CAPACITY_FORCE_UNRESOLVED_INDEX") ?
          select_capacity_replacement (
            gallery, &probe_view, selected, relations,
            gallery->subtemplates->len, &capacity_replacement, FALSE,
            analyzer_capacity_relation_builder) :
          gdix51c0_chicago_enrollment_select_capacity_replacement (
            gallery, &probe_view, selected, relations,
            gallery->subtemplates->len,
            analyzer_capacity_relation_builder,
            &capacity_replacement)))
    return 6;
  if (g_getenv ("CHICAGO_CAPACITY_SELECT"))
    printf ("native-capacity-replacement=%u\n", capacity_replacement);
  if (g_getenv ("CHICAGO_CAPACITY_RECONSTRUCT") &&
      !gdix51c0_chicago_enrollment_synthesize_capacity_relations (
        gallery, NULL, gallery->subtemplates->len))
    return 6;
  if (g_getenv ("CHICAGO_CAPACITY_PROPAGATE") &&
      !gdix51c0_chicago_enrollment_propagate_capacity_relations (
        gallery, relations, gallery->subtemplates->len,
        &capacity_unresolved))
    return 6;
  if (g_getenv ("CHICAGO_CAPACITY_PROPAGATE"))
    printf ("native-capacity-propagate unresolved=%u\n",
            capacity_unresolved);
  if (g_getenv ("CHICAGO_CAPACITY_SYNTHESIZE") &&
      !gdix51c0_chicago_enrollment_synthesize_capacity_relations (
        gallery, relations, gallery->subtemplates->len))
    return 6;
  if (g_getenv ("CHICAGO_CAPACITY_GALLERY_RELATIONS_OUT") &&
      !g_file_set_contents (
        g_getenv ("CHICAGO_CAPACITY_GALLERY_RELATIONS_OUT"),
        gallery->relations->data,
        gallery->relations->len * sizeof (*relations), &error))
    goto fail;
  if (g_getenv ("CHICAGO_CAPACITY_RELATIONS_OUT") &&
      !g_file_set_contents (g_getenv ("CHICAGO_CAPACITY_RELATIONS_OUT"),
                            (const gchar *) relations,
                            gallery->subtemplates->len * sizeof (*relations),
                            &error))
    goto fail;
  if (gallery->subtemplates->len == gallery->capacity)
    {
      if (!gdix51c0_chicago_enrollment_calculate_capacity_scores (
            gallery, &probe_view, relations, gallery->subtemplates->len,
            &capacity_probe_score, capacity_scores, capacity_qualities))
        return 6;
      printf ("native-capacity-selector threshold=%d scores=",
              capacity_probe_score);
      for (guint index = 0; index < gallery->subtemplates->len; index++)
        printf ("%s%d", index ? "," : "", capacity_scores[index]);
      printf (" secondary=");
      for (guint index = 0; index < gallery->subtemplates->len; index++)
        printf ("%s%d", index ? "," : "", capacity_qualities[index]);
      printf ("\n");
    }
  if (gallery->subtemplates->len < gallery->capacity)
    {
      if (!gdix51c0_chicago_enrollment_append_study (
            gallery, &probe_view, selected, relations,
            gallery->subtemplates->len, NULL, &error))
        goto fail;
    }
  else if (!gdix51c0_chicago_enrollment_replace_study (
             gallery, &probe_view, selected,
             (g_getenv ("CHICAGO_CAPACITY_SELECT") &&
              capacity_replacement != G_MAXUINT) ?
               capacity_replacement : selected,
             relations,
             gallery->subtemplates->len, &error))
    goto fail;
  packed = gdix51c0_chicago_enrollment_pack (gallery, &error);
  if (!packed)
    goto fail;
  packed_data = g_bytes_get_data (packed, &packed_size);
  if (!g_file_set_contents (argv[4], packed_data, packed_size, &error))
    goto fail;
  printf ("native-study-mutate selected=%u count=%u packed=%zu output=%s\n",
          selected, gallery->subtemplates->len, packed_size, argv[4]);
  return 0;

fail:
  fprintf (stderr, "native study append failed: %s\n",
           error ? error->message : "invalid input");
  return 1;
}
