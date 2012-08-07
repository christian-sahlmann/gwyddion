/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define GWY_TYPE_NEURAL_NETWORK             (gwy_neural_network_get_type())
#define GWY_NEURAL_NETWORK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_NEURAL_NETWORK, GwyNeuralNetwork))
#define GWY_NEURAL_NETWORK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_NEURAL_NETWORK, GwyNeuralNetworkClass))
#define GWY_IS_NEURAL_NETWORK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_NEURAL_NETWORK))
#define GWY_IS_NEURAL_NETWORK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_NEURAL_NETWORK))
#define GWY_NEURAL_NETWORK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_NEURAL_NETWORK, GwyNeuralNetworkClass))

typedef struct _GwyNeuralNetwork      GwyNeuralNetwork;
typedef struct _GwyNeuralNetworkClass GwyNeuralNetworkClass;

typedef struct {
    guint nlayers;  /* number of hidden layers, fixed to 1 now */
    guint width;
    guint height;
    guint nhidden;
    guint noutput;   /* fixed to 1 */

    gdouble *whidden;
    gdouble *winput;

    guint inpowerxy;
    guint inpowerz;
    gchar *outunits;
} NeuralNetworkData;

struct _GwyNeuralNetwork {
    GwyResource parent_instance;
    NeuralNetworkData data;

    /* forward feed data */
    gdouble *input;
    gdouble *hidden;
    gdouble *output;

    /* training data */
    gdouble *dhidden;
    gdouble *doutput;
    gdouble *target;
    gdouble *wphidden;
    gdouble *wpinput;
};

struct _GwyNeuralNetworkClass {
    GwyResourceClass parent_class;
};

static GType             gwy_neural_network_get_type(void)                          G_GNUC_CONST;
static void              gwy_neural_network_finalize(GObject *object);
static void              gwy_neural_network_use     (GwyResource *resource);
static void              gwy_neural_network_release (GwyResource *resource);
static GwyNeuralNetwork* gwy_neural_network_new     (const gchar *name,
                                                     const NeuralNetworkData *data,
                                                     gboolean is_const);
static void              gwy_neural_network_dump    (GwyResource *resource,
                                                     GString *str);
static GwyResource*      gwy_neural_network_parse   (const gchar *text,
                                                     gboolean is_const);
static void              neural_network_data_resize (NeuralNetworkData *nndata);
static void              neural_network_data_init   (NeuralNetworkData *nndata,
                                                     GRand *rng);
static void              neural_network_data_copy   (const NeuralNetworkData *src,
                                                     NeuralNetworkData *dest);
static void              neural_network_data_free   (NeuralNetworkData *nndata);

static const NeuralNetworkData neuralnetworkdata_default = {
    1, 11, 11, 7, 1,  /* sizes */
    NULL, NULL,       /* weights */
    0, 1, NULL,       /* units */
};

G_DEFINE_TYPE(GwyNeuralNetwork, gwy_neural_network, GWY_TYPE_RESOURCE)

static void
gwy_neural_network_class_init(GwyNeuralNetworkClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    gobject_class->finalize = gwy_neural_network_finalize;

    parent_class = GWY_RESOURCE_CLASS(gwy_neural_network_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);

    res_class->name = "neuralnetwork";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    res_class->use = gwy_neural_network_use;
    res_class->release = gwy_neural_network_release;
    res_class->dump = gwy_neural_network_dump;
    res_class->parse = gwy_neural_network_parse;
}

static void
gwy_neural_network_init(GwyNeuralNetwork *nn)
{
    gwy_debug_objects_creation(G_OBJECT(nn));
    nn->data = neuralnetworkdata_default;
    neural_network_data_resize(&nn->data);
}

static void
gwy_neural_network_resize(GwyNeuralNetwork *nn)
{
    NeuralNetworkData *nndata = &nn->data;
    guint ninput = nndata->width * nndata->height;

    /* Must be called separately: neural_network_data_resize(nndata); */

    nn->input = g_renew(gdouble, nn->input, ninput);
    nn->hidden = g_renew(gdouble, nn->hidden, nndata->nhidden);
    nn->output = g_renew(gdouble, nn->output, nndata->noutput);
    nn->target = g_renew(gdouble, nn->target, nndata->noutput);

    nn->dhidden = g_renew(gdouble, nn->dhidden, nndata->nhidden);
    nn->doutput = g_renew(gdouble, nn->doutput, nndata->noutput);
    gwy_clear(nn->dhidden, nndata->nhidden);
    gwy_clear(nn->doutput, nndata->noutput);

    nn->wpinput = g_renew(gdouble, nn->wpinput,
                          (ninput + 1)*nndata->nhidden);
    nn->wphidden = g_renew(gdouble, nn->wphidden,
                           (nndata->nhidden + 1)*nndata->noutput);
    gwy_clear(nn->wpinput, (ninput + 1)*nndata->nhidden);
    gwy_clear(nn->wphidden, (nndata->nhidden + 1)*nndata->noutput);
}

static void
gwy_neural_network_finalize(GObject *object)
{
    GwyNeuralNetwork *nn;

    nn = GWY_NEURAL_NETWORK(object);
    neural_network_data_free(&nn->data);

    G_OBJECT_CLASS(gwy_neural_network_parent_class)->finalize(object);
}

static void
gwy_neural_network_use(GwyResource *resource)
{
    GwyNeuralNetwork *nn = GWY_NEURAL_NETWORK(resource);
    gwy_neural_network_resize(nn);
}

static void
gwy_neural_network_release(GwyResource *resource)
{
    GwyNeuralNetwork *nn = GWY_NEURAL_NETWORK(resource);

    g_free(nn->input);
    g_free(nn->hidden);
    g_free(nn->output);
    g_free(nn->dhidden);
    g_free(nn->doutput);
    g_free(nn->wpinput);
    g_free(nn->wphidden);
    nn->input = nn->hidden = nn->output = nn->dhidden = nn->doutput = NULL;
    nn->wpinput = nn->wphidden = NULL;
}

static GwyNeuralNetwork*
gwy_neural_network_new(const gchar *name,
                       const NeuralNetworkData *data,
                       gboolean is_const)
{
    GwyNeuralNetwork *nn;

    nn = g_object_new(GWY_TYPE_NEURAL_NETWORK,
                      "is-const", is_const,
                      NULL);
    neural_network_data_copy(data, &nn->data);
    g_string_assign(GWY_RESOURCE(nn)->name, name);
    /* New non-const resources start as modified */
    GWY_RESOURCE(nn)->is_modified = !is_const;

    return nn;
}

static void
gwy_neural_network_write_weights(const gdouble *weights,
                                 guint n,
                                 GString *str)
{
    g_string_set_size(str, str->len + (G_ASCII_DTOSTR_BUF_SIZE + 1)*n);

    while (n--) {
        gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
        g_ascii_dtostr(buf, sizeof(buf), *weights);
        g_string_append(str, buf);
        g_string_append_c(str, n ? ' ' : '\n');
        weights++;
    }
}

static void
gwy_neural_network_dump(GwyResource *resource,
                        GString *str)
{
    GwyNeuralNetwork *nn;
    NeuralNetworkData *nndata;
    gchar *outunits;
    guint ninput;

    g_return_if_fail(GWY_IS_NEURAL_NETWORK(resource));
    nn = GWY_NEURAL_NETWORK(resource);
    nndata = &nn->data;
    ninput = nndata->width * nndata->height;
    outunits = g_strescape(nndata->outunits, NULL);

    /* Information */
    g_string_append_printf(str,
                           "width %u\n"
                           "height %u\n"
                           "nlayers %u\n"
                           "nhidden %u\n"
                           "noutput %u\n"
                           "xyunitpower %u\n"
                           "zunitpower %u\n"
                           "outunits \"%s\"\n",
                           nndata->width, nndata->height,
                           nndata->nlayers, nndata->nhidden, nndata->noutput,
                           nndata->inpowerxy, nndata->inpowerz,
                           outunits);
    g_free(outunits);

    gwy_neural_network_write_weights(nndata->winput,
                                     (ninput + 1)*nndata->nhidden,
                                     str);
    gwy_neural_network_write_weights(nndata->whidden,
                                     (nndata->nhidden + 1)*nndata->noutput,
                                     str);
}

static void
gwy_neural_network_read_weights(gdouble *weights,
                                guint n,
                                const gchar *s)
{
    gchar *end;

    while (n--) {
        *weights = g_ascii_strtod(s, &end);
        s = end;
        weights++;
    }
}

static GwyResource*
gwy_neural_network_parse(const gchar *text,
                         gboolean is_const)
{
    NeuralNetworkData nndata;
    GwyNeuralNetwork *nn = NULL;
    GwyNeuralNetworkClass *klass;
    gchar *str, *p, *line, *key, *value;
    guint len, layer = 0;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_NEURAL_NETWORK);
    g_return_val_if_fail(klass, NULL);

    neural_network_data_init(&nndata, NULL);
    p = str = g_strdup(text);
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        key = line;
        if (!*key)
            continue;

        if (g_ascii_isdigit(key[0]) || key[0] == '-' || key[0] == '+') {
            if (layer == 0) {
                guint ninput = nndata.width * nndata.height;
                gwy_neural_network_read_weights(nndata.winput,
                                                (ninput + 1)*nndata.nhidden,
                                                line);
            }
            else if (layer == 1)
                gwy_neural_network_read_weights(nndata.whidden,
                                                (nndata.nhidden + 1)*nndata.noutput,
                                                line);
            else {
                g_warning("Too many neuron weight lines.");
            }
            layer++;
            continue;
        }

        value = strchr(key, ' ');
        if (value) {
            *value = '\0';
            value++;
            g_strstrip(value);
        }
        if (!value || !*value) {
            g_warning("Missing value for `%s'.", key);
            continue;
        }

        /* Information */
        else if (gwy_strequal(key, "width"))
            nndata.width = atoi(value);
        else if (gwy_strequal(key, "height"))
            nndata.height = atoi(value);
        else if (gwy_strequal(key, "nlayers")) {
            nndata.nlayers = atoi(value);
            if (nndata.nlayers != 1)
                g_warning("Number of neural network layers must be 1.");
        }
        else if (gwy_strequal(key, "nhidden"))
            nndata.nhidden = atoi(value);
        else if (gwy_strequal(key, "noutput")) {
            nndata.noutput = atoi(value);
            if (nndata.noutput != 1)
                g_warning("Neural network output length must be 1.");
        }
        else if (gwy_strequal(key, "xyunitpower"))
            nndata.inpowerxy = atoi(value);
        else if (gwy_strequal(key, "zunitpower"))
            nndata.inpowerz = atoi(value);
        else if (gwy_strequal(key, "outunits")) {
            len = strlen(value);
            if (value[0] == '"' && len >= 2 && value[len-1] == '"') {
                value[len-1] = '\0';
                value++;
                g_free(nndata.outunits);
                nndata.outunits = g_strcompress(value);
            }
        }
        else
            g_warning("Unknown field `%s'.", key);
    }

    nn = gwy_neural_network_new("", &nndata, is_const);
    GWY_RESOURCE(nn)->is_modified = FALSE;
    /* FIXME: gwy_neural_network_data_sanitize(&nn->data); */
    g_free(str);
    neural_network_data_free(&nndata);

    return (GwyResource*)nn;
}

static GwyInventory*
gwy_neural_networks(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek
                                        (GWY_TYPE_NEURAL_NETWORK))->inventory;
}

static void
neural_network_data_resize(NeuralNetworkData *nndata)
{
    guint ninput = nndata->width * nndata->height;

    nndata->winput = g_renew(gdouble, nndata->winput,
                             (ninput + 1)*nndata->nhidden);
    nndata->whidden = g_renew(gdouble, nndata->whidden,
                              (nndata->nhidden + 1)*nndata->noutput);
    neural_network_data_init(nndata, NULL);
}

static void
neural_network_data_init(NeuralNetworkData *nndata,
                         GRand *rng)
{
    GRand *myrng = rng ? rng : g_rand_new();
    guint ninput = nndata->width * nndata->height, i;
    gdouble *p;

    gwy_clear(nndata, 1);

    for (i = (ninput + 1)*nndata->nhidden, p = nndata->winput; i; i--, p++)
        *p = (2.0*g_rand_double(myrng) - 1.0)*0.1;

    for (i = (nndata->nhidden + 1)*nndata->noutput, p = nndata->whidden;
         i;
         i--, p++)
        *p = (2.0*g_rand_double(myrng) - 1.0)*0.1;

    if (rng)
        g_rand_free(rng);

    nndata->outunits = g_strdup("");
}

static void
neural_network_data_copy(const NeuralNetworkData *src,
                         NeuralNetworkData *dest)
{
    guint ninput = src->width * src->height;

    g_return_if_fail(src != (const NeuralNetworkData*)dest);
    g_free(dest->outunits);
    g_free(dest->winput);
    g_free(dest->whidden);
    *dest = *src;
    dest->outunits = g_strdup(dest->outunits ? dest->outunits : "");
    dest->winput = g_memdup(dest->winput,
                            (ninput + 1)*dest->nhidden*sizeof(gdouble));
    dest->whidden = g_memdup(dest->whidden,
                             (dest->nhidden + 1)*dest->noutput*sizeof(gdouble));
}

static void
neural_network_data_free(NeuralNetworkData *nndata)
{
    g_free(nndata->winput);
    g_free(nndata->whidden);
    g_free(nndata->outunits);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
